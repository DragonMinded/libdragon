#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include "system.h"
#include "nand.h"
#include "bbfs.h"

#ifndef N64
#define be16(x)   __builtin_bswap16(x)
#define be32(x)   __builtin_bswap32(x)
#else
#define be16(x)   (x)
#define be32(x)   (x)
#endif

#define FOURCC(d, c, b, a)      ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define FAT_TERMINATOR          -1
#define FAT_BADBLOCK            -2
#define FAT_RESERVED            -3

#define BBFS_MAX_ENTRIES        409

/** @brief Superblock footer */
typedef struct __attribute__((packed)) {
    uint32_t magic;         ///< 'BBFS' or 'BBFL'
    uint32_t seqno;         ///< Sequence number; bigger is newer
    uint16_t link;          ///< Link to next superblock (for NANDs larger than 64 MiB)
    uint16_t checksum;      ///< Checksum (16-bit sum of all 16-bit words)
} bbfs_footer_t;

/** @brief A file entry in the BBFS filesystem */
typedef struct __attribute__((packed)) {
    char name[8];           ///< Filename (0 terminated if shorter than 8 chars)
    char ext[3];            ///< Extension (0 terminated if shorter than 3 chars)
    uint8_t valid;          ///< 1 if entry is valid
    int16_t block;          ///< First block of the file
    uint16_t padding;       ///< Padding
    uint32_t size;          ///< File size in bytes
} bbfs_entry_t;

/** @brief BBFS superblock */
typedef struct {
    int16_t fat[4096];                          ///< Block allocation table
    bbfs_entry_t entries[BBFS_MAX_ENTRIES];     ///< File entries
    bbfs_footer_t footer;                       ///< Footer
} bbfs_superblock_t; 

/** @brief An open file in the BBFS filesystem */
typedef struct {
    bbfs_entry_t *entry;    ///< File entry
    int32_t pos;            ///< Current file position
    int16_t block;          ///< Current block
} bbfs_openfile_t;

_Static_assert(sizeof(bbfs_superblock_t) == NAND_BLOCK_SIZE, "bbfs_superblock_t size mismatch");

static bbfs_superblock_t bbfs_superblock;

static int bbfs_mount(void)
{
    bbfs_footer_t footers[16];
    int nf = 0;

    for (int i=0; i<16; i++) {
        bbfs_footer_t *f = &footers[nf];
        nand_read_data(NAND_ADDR_MAKE(0xFF0 + i, 0, 0x3FF4), f, 12);
        #ifndef N64
        f->magic = __builtin_bswap32(f->magic);
        f->seqno = __builtin_bswap32(f->seqno);
        f->link = __builtin_bswap16(f->link);
        f->checksum = __builtin_bswap16(f->checksum);
        #endif

        if (f->magic == FOURCC('B', 'B', 'F', 'S') || f->magic == FOURCC('B', 'B', 'F', 'L')) {
            f->magic = i;
            nf++;
        }
    }

    if (nf == 0) {
        return BBFS_ERR_SUPERBLOCK;
    }

    // Sort footers by seqno, so that we can use the most recent one
    for (int i=1; i<nf; i++) {
        bbfs_footer_t key = footers[i];
        int j = i-1;
        while (j >= 0 && footers[j].seqno > key.seqno) {
            footers[j+1] = footers[j];
            j--;
        }
        footers[j+1] = key;
    }

    // We now want to find the most recent one, making sure the checksum
    // is correct.
    for (int i=0; i<nf; i++) {
        int idx = footers[i].magic;
        nand_read_data(NAND_ADDR_MAKE(0xFF0 + idx, 0, 0), &bbfs_superblock, sizeof(bbfs_superblock));

        // Verify superblock checksum (16-bit sum of all 16-bit words)
        uint16_t checksum = 0;
        uint16_t *data = (uint16_t *)&bbfs_superblock;
        for (int j=0; j<NAND_BLOCK_SIZE/2; j++)
            checksum += be16(data[j]);
        if (checksum == 0xCAD7) {
            assert(footers[i].link == 0);
            return 0;
        }
    }

    return BBFS_ERR_SUPERBLOCK;
}

static bbfs_entry_t *bbfs_find_entry(const char *name)
{
    char *dot = strchr(name, '.');
    if (dot - name > 8 || (dot && strlen(dot+1) > 3))
        return NULL;
    int namelen = dot ? dot-name : strlen(name);

    for (int i=0; i<BBFS_MAX_ENTRIES; i++) {
        bbfs_entry_t *entry = &bbfs_superblock.entries[i];
        if (entry->valid && !strncmp(entry->name, name, namelen) && !strncmp(entry->ext, dot ? dot+1 : "", 3)) {
            return entry;
        }
    }

    return NULL;
}

static void *__bbfs_open(char *name, int flags)
{
    // Write access not implemented yet
    if (flags == O_WRONLY || flags == O_RDWR)
        return NULL;

    // Search for the file
    bbfs_entry_t *entry = bbfs_find_entry(name);
    if (entry) {
        bbfs_openfile_t *file = malloc(sizeof(bbfs_openfile_t));
        file->entry = entry;
        file->pos = 0;
        file->block = be16(entry->block);
        return file;
    }

    return NULL;
}

static int __bbfs_close(void *file)
{
    free(file);
    return 0;
}

static int __bbfs_read(void *file, uint8_t *ptr, int len)
{
    bbfs_openfile_t *f = file;
    int block = f->block;
    int pos = f->pos;
    int size = be32(f->entry->size);

    if (pos >= size)
        return 0;

    int toread = size - pos;
    if (toread > len)
        toread = len;

    int read = 0;
    while (toread > 0) {
        int offset = pos % NAND_BLOCK_SIZE;
        int n = NAND_BLOCK_SIZE - offset;
        if (n > toread)
            n = toread;

        nand_read_data(NAND_ADDR_MAKE(block, 0, offset), ptr, n);
        ptr += n;
        pos += n;
        toread -= n;
        read += n;

        if (pos % NAND_BLOCK_SIZE == 0) {
            block = be16(bbfs_superblock.fat[block]);
        }
    }

    f->block = block;
    f->pos = pos;
    return read;
}

static int __bbfs_lseek(void *file, int offset, int whence)
{
    bbfs_openfile_t *f = file;
    int size = be32(f->entry->size);
    int pos = f->pos;
    int block = f->block;

    switch (whence) {
        case SEEK_SET:
            pos = offset;
            break;
        case SEEK_CUR:
            pos += offset;
            break;
        case SEEK_END:
            pos = size + offset;
            break;
    }

    if (pos < 0)
        pos = 0;
    if (pos > size)
        pos = size;

    // Check if the current block changed
    if (pos / NAND_BLOCK_SIZE != f->pos / NAND_BLOCK_SIZE) {
        block = be16(f->entry->block);
        int newpos = pos;
        while (newpos >= NAND_BLOCK_SIZE) {
            block = be16(bbfs_superblock.fat[block]);
            newpos -= NAND_BLOCK_SIZE;
        }
    }

    f->pos = pos;
    f->block = block;
    return pos;
}

static int __bbfs_fstat(void *file, struct stat *st)
{
    bbfs_openfile_t *f = file;
    memset(st, 0, sizeof(struct stat));
    st->st_ino = f->entry - bbfs_superblock.entries;
    st->st_mode = S_IFREG;
    st->st_size = be32(f->entry->size);
    st->st_blksize = NAND_BLOCK_SIZE;
    st->st_blocks = (st->st_size + NAND_BLOCK_SIZE - 1) / NAND_BLOCK_SIZE;
    return 0;
}

static int __bbfs_findnext(dir_t *dir)
{
    for (int i=dir->d_cookie+1; i<BBFS_MAX_ENTRIES; i++) {
        bbfs_entry_t *entry = &bbfs_superblock.entries[i];
        if (entry->valid) {
            dir->d_cookie = i;
            dir->d_type = DT_REG;
            
            int j=0;
            for (int k=0; k<8; k++) {
                if (entry->name[k] == 0) break;
                dir->d_name[j++] = entry->name[k];
            }
            dir->d_name[j++] = '.';
            for (int k=0; k<3; k++) {
                if (entry->ext[k] == 0) break;
                dir->d_name[j++] = entry->ext[k];
            }
            dir->d_name[j++] = '\0';
            return 0;
        }
    }

    dir->d_cookie = BBFS_MAX_ENTRIES;
    return -1;
}

static int __bbfs_findfirst(char *name, dir_t *dir)
{
    if (!name || strcmp(name, "/"))
        return -1;
 
    dir->d_cookie = -1;
    return __bbfs_findnext(dir);
}

static filesystem_t bb_fs = {
	.open = __bbfs_open,
	.fstat = __bbfs_fstat,
	.lseek = __bbfs_lseek,
	.read = __bbfs_read,
	.write = 0, //__bbfs_write,
	.close = __bbfs_close,
	.unlink = 0, //__bbfs_unlink,
	.findfirst = __bbfs_findfirst,
	.findnext = __bbfs_findnext,
};

int bbfs_init(void)
{
    nand_init();

    int err = bbfs_mount();
    if (err < 0)
        return err;

    attach_filesystem("bbfs", &bb_fs);
    return 0;
}

int16_t* bbfs_get_file_blocks(const char *filename)
{
    bbfs_entry_t* entry = bbfs_find_entry(filename);
    if (!entry)
        return NULL;

    int num_blocks = (be32(entry->size) + NAND_BLOCK_SIZE - 1) / NAND_BLOCK_SIZE;
    int16_t *blocks = malloc(sizeof(int16_t) * (num_blocks + 1));

    int block = be16(entry->block);
    for (int i=0; i<num_blocks; i++) {
        if (block < 0 || block > 4095) {
            // Corrupted filesystem
            free(blocks);
            return NULL;
        }
        blocks[i] = block;
        block = be16(bbfs_superblock.fat[block]);
    }

    blocks[num_blocks] = -1;
    return blocks;
}
