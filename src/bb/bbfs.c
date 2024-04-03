#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "debug.h"
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

/** @brief Calculate a random number in range [0..n), assuming RAND_MAX = (1<<31)-1 */
#define RANDN(n)                (((uint64_t)rand() * (n)) >> 31)

#define FOURCC(d, c, b, a)      ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define FAT_UNUSED               0
#define FAT_TERMINATOR          -1
#define FAT_BADBLOCK            -2
#define FAT_RESERVED            -3

#define BBFS_MAX_ENTRIES        409

#define BBFS_FLAGS_READING          (1<<0)     ///< The file is open for reading
#define BBFS_FLAGS_WRITING          (1<<1)     ///< The file is open for writing
#define BBFS_FLAGS_PAGE_CACHED      (1<<2)     ///< The current page is cached in the page cache
#define BBFS_FLAGS_BLOCK_SHADOWED   (1<<3)     ///< The current block is now duplicated into a new block
#define BBFS_BIGFILE_THRESHOLD   (512*1024)  ///< Files bigger than this are stored in the beginning of the filesystem

/** @brief Superblock footer */
typedef struct __attribute__((packed)) {
    uint32_t magic;         ///< 'BBFS' or 'BBFL'
    uint32_t seqno;         ///< Sequence number; bigger is newer
    uint16_t link;          ///< Link to next superblock (for NANDs larger than 64 MiB)
    uint16_t checksum;      ///< Checksum (16-bit sum of all 16-bit words)
} bbfs_footer_t;

/** @brief A file entry in the BBFS filesystem */
typedef struct {
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
    bbfs_entry_t *entry;        ///< File entry
    int32_t pos;                ///< Current file position
    int16_t block;              ///< Current block
    int16_t* block_prev_link;   ///< Pointer to the FAT entry that points to this block
    uint32_t page_dirty_mask;   ///< Bitmap of dirty pages in current block
    int8_t flags;               ///< Flags
    uint8_t page_cache[];       ///< Page cache
} bbfs_openfile_t;

_Static_assert(sizeof(bbfs_superblock_t) == NAND_BLOCK_SIZE, "bbfs_superblock_t size mismatch");

/** @brief Running state of the filesystem */
typedef struct {
    uint32_t sb_dirty;          ///< Superblock dirty mask
    int total_blocks;           ///< Total number of blocks in the filesystem
    int small_area_idx;         ///< Start index of the area for small files (end of filesystem)
    int small_area_free;        ///< Number of free blocks in the small area
} bbfs_state_t;

static bbfs_superblock_t bbfs_superblock;
static bbfs_state_t bbfs_state;

static void bbfs_small_area_update(void);

static void bbfs_small_area_init(void)
{
    // The small file area is an area at the end of the filesystem where
    // we store small files that are more likely to be updated / rewritten / changed.
    // Small files are also unlikely to be memory mapped via ATB, which means
    // that fragmentation doesn't really matter for them.
    // We calculate the small file area dynamically, making as big as necessary
    // to always have at least 20% of the area itself free, with a minimum of 1 MiB.
    int total_blocks = bbfs_state.total_blocks;

    // Start by counting how many free blocks there are in the last 1 MiB
    int area_size = 1024*1024 / NAND_BLOCK_SIZE;
    int free_blocks = 0;
    for (int i=total_blocks-area_size; i<total_blocks-16; i++) {
        if (bbfs_superblock.fat[i] == FAT_UNUSED)
            free_blocks++;
    }

    bbfs_state.small_area_idx = total_blocks - area_size;
    bbfs_state.small_area_free = free_blocks;
    bbfs_small_area_update();
}

static void bbfs_small_area_update(void)
{
    int total_blocks = bbfs_state.total_blocks;

    // Go back increasing the area size until at least 20% of the area is free.
    while (bbfs_state.small_area_free * 5 < total_blocks - bbfs_state.small_area_idx) {
        if (bbfs_state.small_area_idx == 0)
            break;
        bbfs_state.small_area_idx--;
        if (bbfs_superblock.fat[bbfs_state.small_area_idx] == FAT_UNUSED)
            bbfs_state.small_area_free++;
    }
}

static int bbfs_small_area_alloc(void)
{
    int total_blocks = bbfs_state.total_blocks;
    int small_area_size = total_blocks - bbfs_state.small_area_idx;

    // Within the small area, fragmentation doesn't matter. Allocate a random
    // block so that we can reduce wear leveling.
    int block = RANDN(small_area_size) + bbfs_state.small_area_idx;
    for (int i=0; i<small_area_size; i++) {
        if (bbfs_superblock.fat[block] == FAT_UNUSED) {
            bbfs_state.small_area_free--;
            bbfs_small_area_update();
            return block;
        }
        block = block + 1;
        if (block >= total_blocks)
            block = bbfs_state.small_area_idx;
    }

    // No free blocks in the small area; this can only happen if the disk is
    // completely full.
    return -1;
}

static void bbfs_state_init(int nblocks)
{
    memset(&bbfs_state, 0, sizeof(bbfs_state));
    bbfs_state.total_blocks = 4096;
    bbfs_small_area_init();
}

static void sb_record_write(void *data, int len)
{
    int offset = data - (void*)&bbfs_superblock;
    assertf(offset >= 0 && offset < sizeof(bbfs_superblock_t), 
        "internal error: invalid superblock pointer %p (%p)", data, &bbfs_superblock);

    int first_page = offset / NAND_PAGE_SIZE;
    int last_page = (offset + len - 1) / NAND_PAGE_SIZE;
    for (int i=first_page; i<=last_page; i++)
        bbfs_state.sb_dirty |= 1 << i;
}

#define SB_WRITE(a, b)  ({ \
    a = b; \
    sb_record_write(&a, sizeof(a)); \
})

static void sb_flush(void)
{
    if (!bbfs_state.sb_dirty)
        return;
    
    // Update sequence number
    SB_WRITE(bbfs_superblock.footer.seqno, be32(be32(bbfs_superblock.footer.seqno) + 1));

    // Recalculate checksum
    uint16_t checksum = 0;
    uint16_t *data = (uint16_t *)&bbfs_superblock;
    for (int i=0; i<NAND_BLOCK_SIZE/2-1; i++)
        checksum += be16(data[i]);
    checksum = 0xCAD7 - checksum;
    SB_WRITE(bbfs_superblock.footer.checksum, be16(checksum));

    // Select a random superblock entry, and erase it
    int block = RANDN(16) + 0xFF0;
    nand_erase_block(NAND_ADDR_MAKE(block, 0, 0));

    // TODO: we could use the dirty mask here to switch between writing new data
    // and copying data from the previous superblock index.
    nand_write_data(NAND_ADDR_MAKE(block, 0, 0), &bbfs_superblock, sizeof(bbfs_superblock));
        
    bbfs_state.sb_dirty = 0;
}

static int bbfs_mount(void)
{
    bbfs_footer_t footers[16];
    int nf = 0;

    // Linked superblocks are not supported for now
    assertf(nand_get_size() == NAND_BLOCK_SIZE * 4096, "Unsupported NAND size");

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
        while (j >= 0 && footers[j].seqno < key.seqno) {
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
            assert(footers[i].link == 0); // linked superblocks not supported for now
            bbfs_state_init(4096);
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
    bbfs_entry_t *entry = bbfs_find_entry(name);

    if (!entry) {
        if (!(flags & O_CREAT)) {
            errno = ENOENT;
            return NULL;
        }

        // Search for an empty entry in the superblock where the file
        // can be written. 
        for (int i=0; i<BBFS_MAX_ENTRIES; i++) {
            entry = &bbfs_superblock.entries[i];
            if (!entry->valid) {
                memset(entry, 0, sizeof(bbfs_entry_t));
                char *dot = strchr(name, '.');
                strncpy(entry->name, name, dot ? dot-name : 8);
                if (dot) strncpy(entry->ext, dot+1, 3);
                entry->valid = 1;
                entry->block = FAT_TERMINATOR;
                sb_record_write(entry, sizeof(bbfs_entry_t));
                break;
            }
        }
        if (!entry->valid) {
            errno = ENOSPC;
            return NULL;
        }
    } else {
        if (flags & (O_CREAT | O_EXCL)) {
            errno = EEXIST;
            return NULL;
        }
    }

    int mode = flags & 7;
    int page_cache = (mode == O_RDWR || mode == O_WRONLY) ? NAND_PAGE_SIZE : 0;
    bbfs_openfile_t *file = malloc(sizeof(bbfs_openfile_t) + page_cache);
    file->entry = entry;
    file->pos = 0;
    file->block_prev_link = &entry->block;
    file->block = be16(entry->block);
    file->flags = 
        ((mode == O_RDONLY || mode == O_RDWR) ? BBFS_FLAGS_READING : 0) |
        ((mode == O_WRONLY || mode == O_RDWR) ? BBFS_FLAGS_WRITING : 0);

    if (flags & O_TRUNC)
        SB_WRITE(entry->size, 0);
    if (flags & O_APPEND)
        file->pos = be32(entry->size);
    return file;
}

static int __bbfs_read(void *file, uint8_t *ptr, int len)
{
    bbfs_openfile_t *f = file;
    int block = f->block;
    int pos = f->pos;
    int size = be32(f->entry->size);

    if (!(f->flags & BBFS_FLAGS_READING)) {
        errno = EBADF;
        return -1;
    }
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

static int __bbfs_allocate_block(int prev_block, bool big_file)
{
    // In general we prefer linear allocation as that is more likely to reduce
    // fragmentation. Check if the next block is free
    int block = prev_block+1;
    if (prev_block != FAT_TERMINATOR && block < 4096 && bbfs_superblock.fat[block] == FAT_UNUSED)
        return block;

    // If the file is small, search for a free block in the small area.
    if (!big_file)
        return bbfs_small_area_alloc();

    // Search linearly for a free block from the beginning of the filesystem.
    // We store there only large files, so the hope is that there is not much
    // fragmentation
    for (int i=0; i<bbfs_state.total_blocks; i++) {
        if (bbfs_superblock.fat[i] == FAT_UNUSED)
            return i;
    }

    // No free blocks: filesystem is full
    return -1;
}


static void __bbfs_write_page_begin(bbfs_openfile_t *f)
{
    if (!(f->flags & BBFS_FLAGS_PAGE_CACHED)) {
        int page_start = f->pos - (f->pos % NAND_PAGE_SIZE);
        nand_read_data(NAND_ADDR_MAKE(be16(*f->block_prev_link), 0, page_start % NAND_BLOCK_SIZE), f->page_cache, NAND_PAGE_SIZE);
        f->flags |= BBFS_FLAGS_PAGE_CACHED;
    }
}

static void __bbfs_write_page_end(bbfs_openfile_t *f)
{
    if (f->flags & BBFS_FLAGS_PAGE_CACHED) {
        int page_start = f->pos - (f->pos % NAND_PAGE_SIZE);
        nand_write_data(NAND_ADDR_MAKE(f->block, 0, page_start % NAND_BLOCK_SIZE), f->page_cache, NAND_PAGE_SIZE);
        f->flags &= ~BBFS_FLAGS_PAGE_CACHED;
    }
}

static int __bbfs_write_block_begin(bbfs_openfile_t *f)
{
    if ((f->flags & BBFS_FLAGS_BLOCK_SHADOWED) == 0) {
        f->block = __bbfs_allocate_block(be16(*f->block_prev_link), be32(f->entry->size) >= BBFS_BIGFILE_THRESHOLD);
        if (f->block < 0) {
            // No free blocks: filesystem is full
            errno = ENOSPC;
            return -1;
        }
        nand_erase_block(NAND_ADDR_MAKE(f->block, 0, 0));
        f->flags |= BBFS_FLAGS_BLOCK_SHADOWED;
    }
    return 0;
}

static void __bbfs_write_block_end(bbfs_openfile_t *f)
{
    if (f->flags & BBFS_FLAGS_BLOCK_SHADOWED) {
        // We've finished writing to the current block. Update the FAT,
        // basically changing it from prev -> old -> next to prev -> new -> next,
        // that is replacing the old block with the new one.
        if (*f->block_prev_link != FAT_TERMINATOR) {
            int next_block = be16(bbfs_superblock.fat[be16(*f->block_prev_link)]);
            SB_WRITE(bbfs_superblock.fat[f->block], next_block);
            SB_WRITE(bbfs_superblock.fat[be16(*f->block_prev_link)], FAT_UNUSED);
            SB_WRITE(*f->block_prev_link, be16(f->block));
        } else {
            // This is a totally new block. We don't do anything besides registering it
            SB_WRITE(*f->block_prev_link, be16(f->block));
            SB_WRITE(bbfs_superblock.fat[f->block], FAT_TERMINATOR);
        }
        f->block_prev_link = &bbfs_superblock.fat[f->block];
        f->flags &= ~BBFS_FLAGS_BLOCK_SHADOWED;
    }
}

static int __bbfs_block_write(bbfs_openfile_t *f, uint8_t *ptr, int len)
{
    int block = f->block;
    int pos = f->pos;
    int size = be32(f->entry->size);

    // Split the write into page-aligned chunks
    int written = 0;
    while (len > 0) {
        int offset = pos % NAND_PAGE_SIZE;
        int n = NAND_PAGE_SIZE - offset;
        if (n > len)
            n = len;

        if (offset == 0 && n == NAND_PAGE_SIZE) {
            // Fast path: write a full page
            assert(!(f->flags & BBFS_FLAGS_PAGE_CACHED));
            nand_write_data(NAND_ADDR_MAKE(block, 0, pos % NAND_BLOCK_SIZE), ptr, n);
        } else {
            // Slow path: read the page, modify it, and write it back
            __bbfs_write_page_begin(f);
            memcpy(f->page_cache + offset, ptr, n);
        }

        // If this write reached the end of the page, finish writing it (flush the cache if any)
        if ((pos+n) % NAND_PAGE_SIZE == 0)
            __bbfs_write_page_end(f);

        ptr += n;
        pos += n;
        len -= n;
        written += n;
    }

    f->pos = pos;
    if (pos > size) {
        SB_WRITE(f->entry->size, be32(pos));
    }

    return written;
}

static int __bbfs_write(void *file, uint8_t *ptr, int len)
{
    bbfs_openfile_t *f = file;

    if (!(f->flags & BBFS_FLAGS_WRITING)) {
        errno = EBADF;
        return -1;
    }

    // Process each block with __bbfs_block_write
    int written = 0;
    while (len > 0) {
        // Shadow the current block if it wasn't already
        if (__bbfs_write_block_begin(f) < 0)
            return -1;

        int offset = f->pos % NAND_BLOCK_SIZE;
        int n = NAND_BLOCK_SIZE - offset;
        if (n > len)
            n = len;

        int w = __bbfs_block_write(f, ptr, n);
        if (w < 0)
            return w;

        ptr += w;
        len -= w;
        written += w;

        // If we reached the end of the block, finish writing it
        if (f->pos % NAND_BLOCK_SIZE == 0)
            __bbfs_write_block_end(f);
    }

    return written;
}

static int __bbfs_lseek(void *file, int offset, int whence)
{
    bbfs_openfile_t *f = file;
    int size = be32(f->entry->size);
    int pos = f->pos;

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

    bool page_changed = pos / NAND_PAGE_SIZE != f->pos / NAND_PAGE_SIZE;
    bool block_changed = pos / NAND_BLOCK_SIZE != f->pos / NAND_BLOCK_SIZE;

    if (f->flags & BBFS_FLAGS_WRITING) {
        if (page_changed) {
            __bbfs_write_page_end(f);
            if (block_changed)
                __bbfs_write_block_end(f);
        }
    }

    // Check if the current block changed
    if (block_changed) {
        f->block_prev_link = &f->entry->block;
        f->block = be16(*f->block_prev_link);
        int newpos = pos;
        while (newpos >= NAND_BLOCK_SIZE) {
            f->block_prev_link = &bbfs_superblock.fat[f->block];
            f->block = be16(*f->block_prev_link);
            newpos -= NAND_BLOCK_SIZE;
        }
    }

    f->pos = pos;
    return pos;
}

static int __bbfs_close(void *file)
{
    bbfs_openfile_t *f = file;
    if (f->flags & BBFS_FLAGS_WRITING) {
        // Flush writing the current page/block
        __bbfs_write_page_end(f);
        __bbfs_write_block_end(f);
        // Write the superblock if dirty
        sb_flush();
    }

    free(file);
    return 0;
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
	.write = __bbfs_write,
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

    attach_filesystem("bbfs:/", &bb_fs);
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
