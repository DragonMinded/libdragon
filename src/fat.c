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
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
	if (fat_disks[pdrv].disk_read && PhysicalAddr(buff) < 0x00800000)
		return fat_disks[pdrv].disk_read(buff, sector, count);
	if (fat_disks[pdrv].disk_read_sdram && io_accessible(PhysicalAddr(buff)))
		return fat_disks[pdrv].disk_read_sdram(buff, sector, count);
	return RES_PARERR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
	_Static_assert(FF_MIN_SS == 512, "this function assumes sector size == 512");
	_Static_assert(FF_MAX_SS == 512, "this function assumes sector size == 512");
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

/** Maximum number of FAT files that can be concurrently opened */
#define MAX_FAT_FILES 4
static FIL fat_files[MAX_FAT_FILES] = {0};
static DIR find_dir;

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
	int i;
	for (i=0;i<MAX_FAT_FILES;i++)
		if (fat_files[i].obj.fs == NULL)
			break;
	if (i == MAX_FAT_FILES) {
		errno = EMFILE;
		return NULL;
	}

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

    char *fat_name = alloca(strlen(name)+2+1);
    fat_name[0] = '0'; fat_name[1] = ':'; 
    strcpy(fat_name+2, name);

	FRESULT res = f_open(&fat_files[i], fat_name, fatfs_flags);
	if (res != FR_OK)
	{
		__fresult_set_errno(res);
		fat_files[i].obj.fs = NULL;
		return NULL;
	}
	return &fat_files[i];
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

static int __fat_stat(char *name, struct stat *st)
{
	FILINFO fno;
	FRESULT res = f_stat(name, &fno);
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
	FRESULT res = f_close(file);
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

static int __fat_unlink(char *name)
{
	FRESULT res = f_unlink(name);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static int __fat_findnext(dir_t *dir)
{
	FILINFO info;
	FRESULT res = f_readdir(&find_dir, &info);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -2;
	}

	// Check if we reached the end of the directory
	if (info.fname[0] == 0) {
		res = f_closedir(&find_dir);
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
    char *fat_name = alloca(strlen(name)+2+1);
    fat_name[0] = '0'+volid; fat_name[1] = ':';
    strcpy(fat_name+2, name);

	FRESULT res = f_opendir(&find_dir, fat_name);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -2;
	}
	return __fat_findnext(dir);
}

static int __fat_mkdir(char *path, mode_t mode)
{
	FRESULT res = f_mkdir(path);
	if (res != FR_OK) {
		__fresult_set_errno(res);
		return -1;
	}
	return 0;
}

static const filesystem_t fat_newlib_fs = {
	.open = NULL,   // will be patched later
	.stat = __fat_stat,
	.fstat = __fat_fstat,
	.lseek = __fat_lseek,
	.read = __fat_read,
	.write = __fat_write,
	.close = __fat_close,
	.unlink = __fat_unlink,
	.findfirst = NULL, // will be patched later
	.findnext = __fat_findnext,
	.mkdir = __fat_mkdir,
};

static void *__fat_open_vol0(char *name, int flags) { return __fat_open(name, flags, 0); }
static void *__fat_open_vol1(char *name, int flags) { return __fat_open(name, flags, 1); }
static void *__fat_open_vol2(char *name, int flags) { return __fat_open(name, flags, 2); }
static void *__fat_open_vol3(char *name, int flags) { return __fat_open(name, flags, 3); }

static int __fat_findfirst_vol0(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 0); }
static int __fat_findfirst_vol1(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 1); }
static int __fat_findfirst_vol2(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 2); }
static int __fat_findfirst_vol3(char *name, dir_t *dir) { return __fat_findfirst(name, dir, 3); }

static const void (*__fat_open_func[4]) = { __fat_open_vol0, __fat_open_vol1, __fat_open_vol2, __fat_open_vol3 };
static const void (*__fat_findfirst_func[4]) = { __fat_findfirst_vol0, __fat_findfirst_vol1, __fat_findfirst_vol2, __fat_findfirst_vol3 };

int fat_mount(const char *prefix, int fatfs_volume_id, const fat_disk_t* disk)
{
    assertf(fatfs_volume_id >= 0 && fatfs_volume_id < FF_VOLUMES, "Invalid volume ID %d", fatfs_volume_id);
    assertf(fat_disks[fatfs_volume_id].disk_initialize, "Volume %d already mounted", fatfs_volume_id);

    fat_disks[fatfs_volume_id] = *disk;

    FATFS *fs = malloc(sizeof(FATFS));
    char path[3] = {'0' + fatfs_volume_id, ':', 0};

    FRESULT err = f_mount(fs, path, 1);
    if (err != FR_OK) {
        __fresult_set_errno(err);
        fat_disks[fatfs_volume_id] = (fat_disk_t){0};
        return -1;
    }

    if (prefix) {
        filesystem_t *fs = malloc(sizeof(filesystem_t));
        memcpy(fs, &fat_newlib_fs, sizeof(filesystem_t));

        fs->open = __fat_open_func[fatfs_volume_id];
        fs->findfirst = __fat_findfirst_func[fatfs_volume_id];

        attach_filesystem(prefix, fs);
    }

    return 0;
}
