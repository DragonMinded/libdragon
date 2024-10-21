#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <time.h>
#include "fat.h"
#include "fatfs/ff.h"
#include "fatfs/ffconf.h"
#include "fatfs/diskio.h"
#include "debug.h"
#include "system.h"
#include "n64sys.h"
#include "dma.h"

static fat_disk_t fat_disks[FF_VOLUMES] = {0};

DSTATUS disk_initialize(BYTE pdrv)
{
	if (fat_disks[pdrv].disk_initialize)
		return fat_disks[pdrv].disk_initialize();
	return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv)
{
	if (fat_disks[pdrv].disk_status)
		return fat_disks[pdrv].disk_status();
	return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
	if (fat_disks[pdrv].disk_read)
		return fat_disks[pdrv].disk_read(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
	if (fat_disks[pdrv].disk_write)
		return fat_disks[pdrv].disk_write(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
	if (fat_disks[pdrv].disk_ioctl)
		return fat_disks[pdrv].disk_ioctl(cmd, buff);
	return RES_PARERR;
}

DWORD get_fattime(void)
{
	time_t t = time(NULL);
	if (t == -1) {
		return (DWORD)(
			(FF_NORTC_YEAR - 1980) << 25 |
			FF_NORTC_MON << 21 |
			FF_NORTC_MDAY << 16
		);
	}
  	struct tm tm = *localtime(&t);
	return (DWORD)(
		(tm.tm_year - 80) << 25 |
		(tm.tm_mon + 1) << 21 |
		tm.tm_mday << 16 |
		tm.tm_hour << 11 |
		tm.tm_min << 5 |
		(tm.tm_sec >> 1)
	);
}

/*********************************************************************
 * FAT newlib wrappers
 *********************************************************************/

#define MAKE_FAT_NAME(volid, name) ({ \
	char *fat_name = alloca(strlen(name)+2+1); \
	fat_name[0] = '0'+volid; fat_name[1] = ':'; \
	strcpy(fat_name+2, name); \
	fat_name; \
})

#define NUM_STATIC_FAT_FILES 4
static FIL static_fat_files[NUM_STATIC_FAT_FILES] = {0};

static void __fresult_set_errno(FRESULT err)
{
	assertf(err != FR_INT_ERR, "FatFS assertion error");
	switch (err) {
	case FR_OK: return;
	case FR_DISK_ERR: 			errno = EIO; return;
	case FR_NOT_READY: 			errno = EBUSY; return;
	case FR_NO_FILE: 			errno = ENOENT; return;
	case FR_NO_PATH: 			errno = ENOENT; return;
	case FR_INVALID_NAME: 		errno = EINVAL; return;
	case FR_DENIED: 			errno = EACCES; return;
	case FR_EXIST: 				errno = EEXIST; return;
	case FR_INVALID_OBJECT: 	errno = EINVAL; return;
	case FR_WRITE_PROTECTED: 	errno = EROFS; return;
	case FR_INVALID_DRIVE: 		errno = ENODEV; return;
	case FR_NOT_ENABLED: 		errno = ENODEV; return;
	case FR_NO_FILESYSTEM: 		errno = ENODEV; return;
	case FR_MKFS_ABORTED: 		errno = EIO; return;
	case FR_TIMEOUT: 			errno = ETIMEDOUT; return;
	case FR_LOCKED: 			errno = EBUSY; return;
	case FR_NOT_ENOUGH_CORE: 	errno = ENOMEM; return;
	case FR_TOO_MANY_OPEN_FILES: errno = EMFILE; return;
	case FR_INVALID_PARAMETER: 	errno = EINVAL; return;
	default: 					errno = EIO; return;
	}
}

static void *__fat_open(char *name, int flags, int volid)
{
	FIL *fp = NULL;
	for (int i=0; i<NUM_STATIC_FAT_FILES; i++)
		if (static_fat_files[i].obj.fs == NULL) {
			fp = &static_fat_files[i];
			break;
		}
	if (!fp)
		fp = malloc(sizeof(FIL));

	int fatfs_flags = 0;
	if ((flags & O_ACCMODE) == O_RDONLY)
		fatfs_flags |= FA_READ;
	if ((flags & O_ACCMODE) == O_WRONLY)
		fatfs_flags |= FA_WRITE;
	if ((flags & O_ACCMODE) == O_RDWR)
		fatfs_flags |= FA_READ | FA_WRITE;
	if ((flags & O_APPEND) == O_APPEND)
		fatfs_flags |= FA_OPEN_APPEND;
	if ((flags & O_TRUNC) == O_TRUNC)
		fatfs_flags |= FA_CREATE_ALWAYS;
	if ((flags & O_CREAT) == O_CREAT) {
		if ((flags & O_EXCL) == O_EXCL)
			fatfs_flags |= FA_CREATE_NEW;
		else
			fatfs_flags |= FA_OPEN_ALWAYS;
	} else
		 fatfs_flags |= FA_OPEN_EXISTING;

	FRESULT res = f_open(fp, MAKE_FAT_NAME(volid, name), fatfs_flags);
	if (res != FR_OK)
	{
		__fresult_set_errno(res);
		if (fp >= static_fat_files && fp < static_fat_files+NUM_STATIC_FAT_FILES)
			fp->obj.fs = NULL;
		else
			free(fp);
		return NULL;
	}
	return fp;
}

static void __fat_stat_fill(FSIZE_t size, BYTE attr, struct stat *st)
{
	memset(st, 0, sizeof(struct stat));
	st->st_size = size;
	if (attr & AM_RDO)
		st->st_mode |= 0444;
	else
		st->st_mode |= 0666;
	if (attr & AM_DIR)
		st->st_mode |= S_IFDIR;
	else
		st->st_mode |= S_IFREG;
}

static int __fat_stat(char *name, struct stat *st, int volid)
{
	FILINFO fno;
	FRESULT res = f_stat(MAKE_FAT_NAME(volid, name), &fno);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	__fat_stat_fill(fno.fsize, fno.fattrib, st);
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	tm.tm_min = (fno.ftime >> 5) & 0x3F;
	tm.tm_hour = fno.ftime >> 11;
	tm.tm_mday = fno.fdate & 0x1F;
	tm.tm_mon = ((fno.fdate >> 5) & 0x0F) - 1;
	tm.tm_year = (fno.fdate >> 9) + 80;
	st->st_mtim.tv_sec = mktime(&tm);
	st->st_mtim.tv_nsec = 0;
	return 0;
}

static int __fat_fstat(void *file, struct stat *st)
{
	FIL *f = file;
	__fat_stat_fill(f_size(f), f->obj.attr, st);
	return 0;
}

static int __fat_read(void *file, uint8_t *ptr, int len)
{
	UINT read;
	FRESULT res = f_read(file, ptr, len, &read);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return read;
}

static int __fat_write(void *file, uint8_t *ptr, int len)
{
	UINT written;
	FRESULT res = f_write(file, ptr, len, &written);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return written;
}

static int __fat_close(void *file)
{
	FIL *fp = file;
	FRESULT res = f_close(fp);

	if (fp >= static_fat_files && fp < static_fat_files+NUM_STATIC_FAT_FILES)
		fp->obj.fs = NULL;
	else
		free(fp);

	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static int __fat_lseek(void *file, int offset, int whence)
{
	FRESULT res;
	FIL *f = file;
	switch (whence)
	{
	case SEEK_SET: res = f_lseek(f, offset); break;
	case SEEK_CUR: res = f_lseek(f, f_tell(f) + offset); break;
	case SEEK_END: res = f_lseek(f, f_size(f) + offset); break;
	default: return -1;
	}
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return f_tell(f);
}

static int __fat_ioctl(void *file, unsigned long request, void *arg)
{
	FIL *f = file;
	switch (request) {
		case IOFAT_GET_CLUSTER: {
			int32_t *cluster = arg;
			*cluster = f->clust;
			return 0;
		}
		case IOFAT_GET_SECTOR: {
			int64_t *sector = arg;
			*sector = f->sect;
			return 0;
		}
		case IOFAT_GET_CLUSTER_SIZE: {
			int32_t *cluster_size = arg;
			*cluster_size = f->obj.fs->csize;
			return 0;
		}
		default: {
			errno = ENOTTY;
			return -1;
		}
	}
}

static int __fat_unlink(char *name, int volid)
{
	FRESULT res = f_unlink(MAKE_FAT_NAME(volid, name));
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static int __fat_findnext(dir_t *dir)
{
	DIR *find_dir = (DIR*)dir->d_cookie;
	FILINFO info;
	FRESULT res = f_readdir(find_dir, &info);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -2;
	}

	// Check if we reached the end of the directory
	if (info.fname[0] == 0) {
		res = f_closedir(find_dir);
		free(find_dir);
		dir->d_cookie = 0;
		if (res != FR_OK) {
			__fresult_set_errno(res);
			return -2;
		}
		return -1;
	}

	strlcpy(dir->d_name, info.fname, sizeof(dir->d_name));
	if (info.fattrib & AM_DIR)
		dir->d_type = DT_DIR;
	else
		dir->d_type = DT_REG;
	dir->d_size = info.fsize;
	return 0;
}

static int __fat_findfirst(char *name, dir_t *dir, int volid)
{
	DIR *find_dir = malloc(sizeof(DIR));
	FRESULT res = f_opendir(find_dir, MAKE_FAT_NAME(volid, name));
	if (res != FR_OK) {
		free(find_dir);
		__fresult_set_errno(res);
		return -2;
	}
	dir->d_cookie = (uint32_t)find_dir;
	return __fat_findnext(dir);
}

static int __fat_mkdir(char *path, mode_t mode, int volid)
{
	FRESULT res = f_mkdir(MAKE_FAT_NAME(volid, path));
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static const filesystem_t fat_newlib_fs = {
	.open = NULL,   		// per-volume function
	.stat = NULL,			// per-volume function
	.fstat = __fat_fstat,
	.lseek = __fat_lseek,
	.read = __fat_read,
	.write = __fat_write,
	.close = __fat_close,
	.ioctl = __fat_ioctl,
	.unlink = NULL, 		// per-volume function
	.findfirst = NULL, 		// per-volume function
	.findnext = __fat_findnext,
	.mkdir = NULL,			// per-volume function
};

static void *__fat_open_vol0(char *name, int flags) { return __fat_open(name, flags, 0); }
static void *__fat_open_vol1(char *name, int flags) { return __fat_open(name, flags, 1); }
static void *__fat_open_vol2(char *name, int flags) { return __fat_open(name, flags, 2); }
static void *__fat_open_vol3(char *name, int flags) { return __fat_open(name, flags, 3); }

static int __fat_findfirst_vol0(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 0); }
static int __fat_findfirst_vol1(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 1); }
static int __fat_findfirst_vol2(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 2); }
static int __fat_findfirst_vol3(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 3); }

static int __fat_stat_vol0(char *name, struct stat *st) { return __fat_stat(name, st, 0); }
static int __fat_stat_vol1(char *name, struct stat *st) { return __fat_stat(name, st, 1); }
static int __fat_stat_vol2(char *name, struct stat *st) { return __fat_stat(name, st, 2); }
static int __fat_stat_vol3(char *name, struct stat *st) { return __fat_stat(name, st, 3); }

static int __fat_unlink_vol0(char *name) { return __fat_unlink(name, 0); }
static int __fat_unlink_vol1(char *name) { return __fat_unlink(name, 1); }
static int __fat_unlink_vol2(char *name) { return __fat_unlink(name, 2); }
static int __fat_unlink_vol3(char *name) { return __fat_unlink(name, 3); }

static int __fat_mkdir_vol0(char *path, mode_t mode) { return __fat_mkdir(path, mode, 0); }
static int __fat_mkdir_vol1(char *path, mode_t mode) { return __fat_mkdir(path, mode, 1); }
static int __fat_mkdir_vol2(char *path, mode_t mode) { return __fat_mkdir(path, mode, 2); }
static int __fat_mkdir_vol3(char *path, mode_t mode) { return __fat_mkdir(path, mode, 3); }

static const void (*__fat_open_func[4]) = { __fat_open_vol0, __fat_open_vol1, __fat_open_vol2, __fat_open_vol3 };
static const void (*__fat_findfirst_func[4]) = { __fat_findfirst_vol0, __fat_findfirst_vol1, __fat_findfirst_vol2, __fat_findfirst_vol3 };
static const void (*__fat_stat_func[4]) = { __fat_stat_vol0, __fat_stat_vol1, __fat_stat_vol2, __fat_stat_vol3 };
static const void (*__fat_unlink_func[4]) = { __fat_unlink_vol0, __fat_unlink_vol1, __fat_unlink_vol2, __fat_unlink_vol3 };
static const void (*__fat_mkdir_func[4]) = { __fat_mkdir_vol0, __fat_mkdir_vol1, __fat_mkdir_vol2, __fat_mkdir_vol3 };

int fat_mount(const char *prefix, const fat_disk_t* disk, int flags)
{
	int vol_id;

	for (vol_id = 0; vol_id < FF_VOLUMES; vol_id++) {
		if (fat_disks[vol_id].disk_initialize == NULL)
			break;
	}
	assertf(vol_id != FF_VOLUMES, "Not enough FAT volumes available (max: %d)", FF_VOLUMES);

    fat_disks[vol_id] = *disk;

    FATFS *fs = malloc(sizeof(FATFS));
    char path[3] = {'0' + vol_id, ':', 0};

    FRESULT err = f_mount(fs, path, (flags & FAT_MOUNT_DEFERRED) ? 0 : 1);
    if (err != FR_OK) {
        __fresult_set_errno(err);
        fat_disks[vol_id] = (fat_disk_t){0};
        return -1;
    }

    if (prefix) {
        filesystem_t *fs = malloc(sizeof(filesystem_t));
        memcpy(fs, &fat_newlib_fs, sizeof(filesystem_t));

        fs->open = __fat_open_func[vol_id];
        fs->findfirst = __fat_findfirst_func[vol_id];
		fs->stat = __fat_stat_func[vol_id];
		fs->unlink = __fat_unlink_func[vol_id];
		fs->mkdir = __fat_mkdir_func[vol_id];

        attach_filesystem(prefix, fs);
    }

    return vol_id;
}
