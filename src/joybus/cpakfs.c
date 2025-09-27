/**
 * @file cpakfs.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak Filesystem Routines
 * @ingroup controllerpak
 */
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "system.h"
#include "cpak.h"
#include "cpakfs.h"
#include "cpakfs_internal.h"
#include "../utils.h"
#ifdef N64
#include "../rand_internal.h"
#include "cop0.h"
#include "debug.h"
#endif

#define FLAG_READING            (1<<0)     ///< Flag to mark a file as being read
#define FLAG_WRITING            (1<<1)     ///< Flag to mark a file as being written
#define FLAG_NOTE_DIRTY         (1<<2)     ///< Flag to mark a file whose note requires being rewritten
#define FLAG_NEW_PAGES          (1<<3)     ///< Flag to mark a file with newly allocated pages that may need cleanup

/** @brief A mounted cpak filesystem */
typedef struct {
    joypad_port_t port;                 ///< Joypad port
    cpakfs_note_t notes[MAX_NOTES];     ///< Cache of all the notes
    uint16_t notes_mask;                ///< Bitmask of read notes
    int cur_bank;                       ///< Current bank for multi-bank cpaks
    uint64_t fat_dirty;                 ///< Dirty mask of FAT
    int fat_size;                       ///< Size of the FAT in bytes
    int reserved;                       ///< Number of reserved pages in the first bank
    int8_t free_pages[MAX_BANKS];       ///< Free pages in each bank (0-62)
    cpakfs_fat_entry_t fat[][NUM_PAGES];///< FAT entries
} cpakfs_t;

/** @brief A open file in the cpakfs */
typedef struct {
    int port;                           ///< Joypad port
    cpakfs_note_t *note;                ///< Note this file is associated with
    int pos;                            ///< Current position in the file
    int size;                           ///< File size
    cpakfs_fat_entry_t *cur_page_ptr;   ///< Pointer to the current page in the FAT (or in the note)
    uint8_t flags;                      ///< Flags
} cpakfs_openfile_t;

/** @brief Modify the FAT and mark it as dirty */
#define FAT_WRITE(fs, lvalue, val) ({ \
    cpakfs_fat_entry_t __val = (val); \
    cpakfs_fat_entry_t *__ptr = &(lvalue); \
    if (__ptr >= &fs->fat[0][0] && __ptr < &fs->fat[0][fs->reserved]) { \
        assertf(0, "FAT_WRITE with value %02x:%02x in reserved area", __val.bank, __val.page); \
    } \
    *__ptr = __val; \
    if (!(__ptr >= &fs->notes[0].first_page && __ptr <= &fs->notes[MAX_NOTES-1].first_page)) { \
        fs->fat_dirty |= 1 << ((__ptr - &fs->fat[0][0]) >> 7); \
    } \
})

static cpakfs_t *filesystems[4];
static char *prefixes[4];

void __cpakfs_fsid_checksum(cpakfs_id_t *id, uint16_t *checksum1, uint16_t *checksum2)
{
    *checksum1 = 0;
    for (int i=0; i<14; i++)
        *checksum1 += be16(id->data16[i]);
    *checksum2 = 0xFFF2 - *checksum1;
}

static int fsid_read(joypad_port_t port, cpakfs_id_t *id)
{
    // Check if one of the ID sectors is correct
    int sectors[4] = { 0x20, 0x60, 0x80, 0xC0 };
    for (int i=0; i<4; i++) {
        if (cpak_read(port, 0, sectors[i], id->data8, 32) < 0) {
            return -1;
        }

        // Verify that FAT size is within the valid range. There are no paks
        // bigger than 2 MiB, and also we currently don't support more than
        // 64 pages (because fat_dirty has 64 bits).
        if ((be16(id->bank_size_msb) & 0xFF00) >= 0x4000)
            continue;

        // Verify checksum
        uint16_t checksum1, checksum2;
        __cpakfs_fsid_checksum(id, &checksum1, &checksum2);
        if (checksum1 == be16(id->checksum1) && checksum2 == be16(id->checksum2))
            return 0;

        // Special case for DexDrive: many dumps have this wrong hard-coded checksum.
        // Just let them pass because the file is otherwise correct.
        if (checksum1 == 0x0101 && be16(id->checksum1) == 0x0101 && be16(id->checksum2) == 0xFEFD)
            return 0;
    }

    // No valid ID sector found
    errno = ENODEV;
    return -1;
}

/** 
 * @brief Compute the checksum of a FAT page, starting from a given entry index. 
 */
uint8_t __cpakfs_fat_checksum(cpakfs_fat_entry_t *fat_page, int start_idx)
{
    uint8_t checksum = 0;
    for (int i=start_idx; i<NUM_PAGES; i++) {
        checksum += fat_page[i].bank;
        checksum += fat_page[i].page;
    }
    return checksum;
}

static int read_fat(cpakfs_t *fs)
{
    const int num_banks = fs->fat_size >> 8;
    const int main_addr = 0x100;
    const int backup_addr = 0x100 + fs->fat_size;

    // Read the main FAT copy
    if (cpak_read(fs->port, 0, main_addr, fs->fat[0], fs->fat_size) < 0)
        return -1;

    for (int i=0; i<num_banks; i++) {
        // Check the checksum of the page
        int csum_start_idx = (i == 0) ? fs->reserved : 1;
        if (__cpakfs_fat_checksum(fs->fat[i], csum_start_idx) != fs->fat[i][0].page) {

            // If the checksum is wrong, read the backup copy and check if the checksum is correct there
            if (cpak_read(fs->port, 0, backup_addr + i*PAGE_SIZE, fs->fat[i], PAGE_SIZE) < 0)
                return -1;
            if (__cpakfs_fat_checksum(fs->fat[i], csum_start_idx) != fs->fat[i][0].page){
                errno = ENODEV;
                return -3;
            }
        }

        // Compute the number of free pages in this bank
        for (int j=csum_start_idx; j<NUM_PAGES; j++) {
            if (FAT_IS_UNUSED(fs->fat[i][j]))
                fs->free_pages[i]++;
        }
    }

    return 0;
}

static int write_fat(cpakfs_t *fs)
{
    const int main_addr = 0x100;
    const int backup_addr = 0x100 + fs->fat_size;

    for (int i=0; i<64 && fs->fat_dirty; i++) {
        if (fs->fat_dirty & (1 << i)) {
            assertf(i < (fs->fat_size >> 8), "FAT page %d is out of bounds (max %d)", i, fs->fat_size >> 8);
            cpakfs_fat_entry_t *fat_page = fs->fat[i];
            tracef("Writing FAT page %d\n", i);

            // Update checksum
            int csum_start_idx = i == 0 ? fs->reserved : 1;
            fat_page[0].page = __cpakfs_fat_checksum(fat_page, csum_start_idx);

            // Write both copies of the FAT page
            if (cpak_write(fs->port, 0, main_addr   + i*PAGE_SIZE, fat_page, PAGE_SIZE) < 0)
                return -1;
            if (cpak_write(fs->port, 0, backup_addr + i*PAGE_SIZE, fat_page, PAGE_SIZE) < 0)
                return -1;

            fs->fat_dirty &= ~(1 << i);
        }
    }
    return 0;
}

static uint16_t crc16(cpakfs_note_t *node)
{
    // Use Koopman polynomy (0x5935) which has HD=5 for up to 30 bytes
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < sizeof(node->data8); ++i) {
        uint16_t tmp = (uint16_t)(crc ^ ((uint16_t)node->data8[i] << 8));
        for (int b = 0; b < 8; ++b) {
            if (tmp & 0x8000u) tmp = (uint16_t)((tmp << 1) ^ 0x5935);
            else               tmp = (uint16_t)(tmp << 1);
        }
        crc = tmp;
    }
    return crc;
}

static cpakfs_note_t* read_note(cpakfs_t *fs, int note_id)
{
    assert(note_id >= 0 && note_id < MAX_NOTES);
    cpakfs_note_t *note = &fs->notes[note_id];
    if (!(fs->notes_mask & (1 << note_id))) {
        int note_start = 0x100 + fs->fat_size*2;
        if (cpak_read(fs->port, 0, note_start + note_id*32, note, sizeof(cpakfs_note_t)) < 0)
            return NULL;
        fs->notes_mask |= 1 << note_id;

        // Sanitize filename and extension by removing all invalid characters.
        // These wouldn't roundtrip to UTF-8 anyway, and they are invalid so better
        // get rid of them from the get-go.
        __cpakfs_n64_string_sanitize(note->filename, 16);
        __cpakfs_n64_string_sanitize(note->ext, 4);

        // We store a crc16 in the note to mark it as using libdragon extension.
        // Basically it's a way to record that the file was written by libdragon.
        // For now, this is useful for one thing: we store the last page's padding size
        // in the note, so that we know the exact size of the file.
        uint16_t crc = be16(note->ext_crc16);
        note->ext_crc16 = 0; // Clear CRC for comparison
        if (crc16(note) != crc) {
            // CRC is correct, the note is not using libdragon extensions.
            // Clear the padding size field as we don't know the exact size
            // so we must assume it is 0.
            note->ext_padding_size = 0;
        }
    }
    return note;
}

/**
 * @brief Find a note matching the given parsed path components
 * 
 * Searches through all notes to find one that matches the given game code,
 * publisher code, filename, and extension.
 * 
 * @param fs        Mounted filesystem
 * @param path      Parsed path components to search for
 * @param note_id   Output pointer to store the found note ID
 * @return cpakfs_note_t* Pointer to the found note, or NULL if not found
 */
static cpakfs_note_t* find_note(cpakfs_t *fs, const cpakfs_path_t *path, int *note_id)
{
    cpakfs_note_t *note = NULL;

    // Hexdump the whole path
    tracef("Searching for note:\n");
    tracef("Game code: %02x%02x%02x%02x\n", path->gamecode[0], path->gamecode[1], path->gamecode[2], path->gamecode[3]);
    tracef("Publisher code: %02x%02x\n", path->pubcode[0], path->pubcode[1]);
    tracef("Filename: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
           path->filename[0], path->filename[1], path->filename[2], path->filename[3],
           path->filename[4], path->filename[5], path->filename[6], path->filename[7],
              path->filename[8], path->filename[9], path->filename[10], path->filename[11],
              path->filename[12], path->filename[13], path->filename[14], path->filename[15]);
    tracef("Extension: %02x%02x%02x%02x\n", path->ext[0], path->ext[1], path->ext[2], path->ext[3]);
    

    for (int i = 0; i < MAX_NOTES; i++) {
        note = read_note(fs, i);
        if (!(note->status & NOTE_STATUS_OCCUPIED))
            continue;

        if (memcmp(note->gamecode, path->gamecode, 4) == 0 &&
            memcmp(note->pubcode, path->pubcode, 2) == 0 &&
            memcmp(note->filename, path->filename, 16) == 0 &&
            memcmp(note->ext, path->ext, 4) == 0) {
            
            tracef("Found note %d for path\n", i);
            if (note_id) *note_id = i;
            return note;
        }
    }
    
    if (note_id) *note_id = MAX_NOTES;
    return NULL;
}

static void truncate_note(cpakfs_t *fs, cpakfs_note_t *note)
{
    cpakfs_fat_entry_t *cur = &note->first_page;
    while (cur) {
        cpakfs_fat_entry_t *next = NULL;
        if (FAT_IS_VALID(*cur, fs->reserved)) {
            next = &FAT_NEXT(fs->fat, *cur);
            fs->free_pages[cur->bank]++;
        } else {
            next = NULL;
        }
        FAT_WRITE(fs, *cur, FAT_UNUSED);
        cur = next;
    }
    // A 0-byte note should have FAT_TERMINATOR as first page
    note->first_page = FAT_TERMINATOR;
}

static int __cpakfs_read(void *file, uint8_t *ptr, int len)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int read = 0;

    tracef("__cpak_read(%p, %p, %d)\n", file, ptr, len);

    if (!(f->flags & FLAG_READING)) {
        tracef("__cpak_read: not reading\n");
        errno = EBADF;
        return -1;
    }

    if (f->pos >= f->size)
        return 0;

    len = MIN(len, f->size - f->pos);
    while (len > 0) {
        // Perform the maximum read operation within the current page
        int page_offset = f->pos % PAGE_SIZE;
        int n = MIN(len, PAGE_SIZE - page_offset);

        // See if we can read multiple pages at once. This is only possible if
        // they are consecutive in the filesystem.
        cpakfs_fat_entry_t *last = f->cur_page_ptr;
        while (n < len) {
            cpakfs_fat_entry_t *next = &FAT_NEXT(fs->fat, *last);
            if (next->bank != last->bank)     break;
            if (next->page != last->page + 1) break;
            n += MIN(len-n, PAGE_SIZE);
            last = next;
        }

        // Perform the read
        if (cpak_read(f->port, f->cur_page_ptr->bank, f->cur_page_ptr->page * PAGE_SIZE + page_offset, ptr, n) < 0)
            return -1;

        // Update counters and optionally move to the next page
        f->pos += n;
        ptr += n;
        len -= n;
        read += n;
        f->cur_page_ptr = last;
        
        if (f->pos % PAGE_SIZE == 0) {
            f->cur_page_ptr = &FAT_NEXT(fs->fat, *f->cur_page_ptr);
        }
    }
    return read;
}

/** 
 * @brief Allocate one page in the filesystem for writing content to it
 *
 * Hardware constraints:
 *  - Bank switching costs one joybus operation, so we want to minimize it, that
 *    is, store a file within the same bank as much as possible.
 *  - Libdragon allows for large consecutive read operations that perform slightly
 *    better than single 32-byte ones (fewer context switches), so all things
 *    equal, it is faster for a file to be stored in consecutive pages.
 *  - Besides this, pages can be randomly accessed, so fragmentation is not
 *    a problem.
 * 
 * This function performs a locality-first selection with random fallback.
 * It works like this:
 * 
 * 1. If the bank and page are specified, it tries to allocate the next page
 *    consecutive to the previous one. This allows to later read files faster
 *    with larger read operations.
 * 2. If the bank is not specified, or the specified bank is full, it selects
 *    the bank with the most free pages available. Without further information,
 *    this gives the more likeness that a file will not be fragmented across
 *    too many different banks.
 * 3. If the bank is specified (or was selected by the previous step), select
 *    a random run of unused pages in the bank, and return the first page of
 *    the run.
 * 
 * @param fs        Mounted filesystem
 * @param bank      Bank to allocate the page in if possible, or -1 if no
 *                  preference is provided
 * @param page      Previous page that was written in the same file if any,
 *                  or -1 if it's the first page of a file.
 * 
 * @return The FAT entry of the allocated page, or FAT_RESERVED if no space is left
 */
static cpakfs_fat_entry_t allocate_page(cpakfs_t *fs, int bank, int page)
{
    // If possible, allocate linearly to speed up reads.
    if (bank >= 0 && page >= 0 && page < 0x7F && FAT_IS_UNUSED(fs->fat[bank][page+1])) {
        fs->free_pages[bank]--;
        return (cpakfs_fat_entry_t){bank, page+1};
    }

    // If no bank affinity was specified (or the current bank is full), select
    // the bank with more free pages available.
    if (bank < 0 || fs->free_pages[bank] == 0) {
        bank = 0;
        for (int i=1; i<fs->fat_size>>8; i++) {
            if (fs->free_pages[i] > fs->free_pages[bank])
                bank = i;
        }
        if (fs->free_pages[bank] <= 0)
            return FAT_RESERVED;
    }

    // The selected bank must have free pages available
    assert(bank >= 0 && bank < (fs->fat_size >> 8));
    assert(fs->free_pages[bank] > 0);

    // Select a random page in the current bank
    page = __randn(NUM_PAGES);

    if (FAT_IS_UNUSED(fs->fat[bank][page])) {
        // If the page is part of a unused run, return the first page of the run
        while (page > 0 && FAT_IS_UNUSED(fs->fat[bank][page-1]))
            page--;
        fs->free_pages[bank]--;
        return (cpakfs_fat_entry_t){bank, page};
    }

    // Otherwise search for the first unused page starting from the current page
    for (int i=0; i<NUM_PAGES; i++) {
        if (FAT_IS_UNUSED(fs->fat[bank][page])) {
            fs->free_pages[bank]--;
            return (cpakfs_fat_entry_t){bank, page};
        }
        page += 1;
        page %= NUM_PAGES;
    }

    // This should never happen, as we already checked that the bank has at least one free page
    assert(0);
}

/**
 * @brief Allocate one next page in a file.
 * 
 * This is meant to be run on the page pointer to the terminator, so that
 * it appends the next page to the chain. The new page will become the new
 * terminator.
 * 
 * @param page_ptr          Pointer to the page that is the terminator
 * @param fs                Filesystem to allocate the page in
 * @return int              0 on success, -1 on error (ENOSPC)
 */
static int allocate_next_page(cpakfs_fat_entry_t *page_ptr, cpakfs_t *fs)
{
    assert(FAT_IS_TERMINATOR(*page_ptr));

    // If the last page is not valid, allocate a new one
    int num_bank = -1, num_page = -1;
    if (page_ptr != &fs->notes[0].first_page) {
        num_bank = (page_ptr - &fs->fat[0][0]) >> 7;
        num_page = (page_ptr - &fs->fat[0][0]) & 0x7F;
    }
    cpakfs_fat_entry_t new_page = allocate_page(fs, num_bank, num_page);
    if (FAT_IS_RESERVED(new_page)) {
        tracef("No space left in the filesystem\n");
        errno = ENOSPC;
        return -1;
    }
    tracef("Allocated next page %02x:%02x\n", new_page.bank, new_page.page);
    FAT_WRITE(fs, *page_ptr, new_page);
    FAT_WRITE(fs, FAT_NEXT(fs->fat, new_page), FAT_TERMINATOR);
    return 0;
}

static int __cpakfs_write(void *file, uint8_t *ptr, int len)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int written = 0;

    tracef("__cpak_write(%p, %p, %d) %d\n", file, ptr, len, errno);

    if (!(f->flags & FLAG_WRITING)) {
        tracef("__cpak_write: not writing\n");
        errno = EBADF;
        return -1;
    }

    while (len > 0) {
        int page_offset = f->pos % PAGE_SIZE;
        int n = MIN(len, PAGE_SIZE - page_offset);

        // If the current page is not valid (eg: beginning of a file, or last
        // write finished exactly at the end of a page), we need to allocate a new page.
        assert(FAT_IS_VALID(*f->cur_page_ptr, fs->reserved) || FAT_IS_TERMINATOR(*f->cur_page_ptr));        
        if (FAT_IS_TERMINATOR(*f->cur_page_ptr)) {
            if (allocate_next_page(f->cur_page_ptr, fs) < 0) return -1;
            f->flags |= FLAG_NEW_PAGES;
        }

        cpakfs_fat_entry_t *last = f->cur_page_ptr;
        while (n < len) {
            // We will need to write more bytes, so allocate immediately one more page
            cpakfs_fat_entry_t *next = &FAT_NEXT(fs->fat, *last);
            if (allocate_next_page(next, fs) < 0) break;
            f->flags |= FLAG_NEW_PAGES;

            // Check if it's consecutive to the current one. If so, just keep writing
            // in the same large write. Otherwise, stop here: the next page is ready
            // for next loop anyway.
            if (next->bank != last->bank)     break;
            if (next->page != last->page + 1) break;

            // Add this page to the batch write
            n += MIN(len - n, PAGE_SIZE);
            last = next;
        }

        tracef("__cpak_write: writing %d bytes to page 0x%02x%02x offset %d\n", n, f->cur_page_ptr->bank, f->cur_page_ptr->page, page_offset);
        if (cpak_write(f->port, f->cur_page_ptr->bank, f->cur_page_ptr->page * PAGE_SIZE + page_offset, ptr, n) < 0)
            return -1;

        f->pos += n;
        ptr += n;
        len -= n;
        written += n;
        f->cur_page_ptr = last;

        if (f->pos % PAGE_SIZE == 0) {
            f->cur_page_ptr = &FAT_NEXT(fs->fat, *f->cur_page_ptr);
        }

        tracef("__cpak_write: pos=%d size=%d left=%d\n", f->pos, f->size, len);
    }

    // If this write increased the file size, record the new size,
    // and mark the note as dirty as we need to update it with the new
    // padding size.
    if (f->pos > f->size) {
        f->size = f->pos;
        f->flags |= FLAG_NOTE_DIRTY;
    }

    return written;
}

static int __cpakfs_lseek(void *file, int offset, int whence)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int size = f->size;
    int pos = f->pos;

    tracef("__cpak_lseek(%p, %d, %d)\n", file, offset, whence);

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

    // Check if the current page changed. If so, update the current page.
    bool page_changed = pos / PAGE_SIZE != f->pos / PAGE_SIZE;
    if (page_changed) {
        int page_idx = pos / PAGE_SIZE;
        f->cur_page_ptr = &f->note->first_page;
        for (int i=0; i<page_idx; i++) {
            if (!FAT_IS_VALID(*f->cur_page_ptr, fs->reserved)) {
                errno = EFTYPE;
                return -1;
            }
            f->cur_page_ptr = &FAT_NEXT(fs->fat, *f->cur_page_ptr);
        }
    }

    f->pos = pos;
    return pos;
}

static int calc_size(cpakfs_t *fs, cpakfs_note_t *note)
{
    // Calculate the size of the file by traversing the FAT chain
    int size = 0;
    cpakfs_fat_entry_t cur_page = note->first_page;
    while (!FAT_IS_TERMINATOR(cur_page)) {
        if (!FAT_IS_VALID(cur_page, fs->reserved) || size > PAGE_SIZE * fs->fat_size) { // prevent infinite loop
            return -1;
        }
        size += PAGE_SIZE;
        cur_page = FAT_NEXT(fs->fat, cur_page);
    }
    // Subtract padding size if any (notice this is always valid,
    // even without libdragon extensions, as we cleared it in that case).
    return size - note->ext_padding_size;
}


static void *__cpakfs_open(char *name, int flags, int port)
{
    cpakfs_path_t path;
    const char *error_pos;

    tracef("__cpak_open(%s, %d, %d) %d\n", name, flags, port, errno);

    // Parse the path
    cpakfs_parse_err_t parse_result = cpakfs_path_parse(name, &path, &error_pos);
    if (parse_result != CPAKFS_PARSE_OK) {
        errno = EINVAL;
        return NULL;
    }

    // Find the note for this file
    cpakfs_t *fs = filesystems[port];
    int note_id;
    cpakfs_note_t *note = find_note(fs, &path, &note_id);

    if (note) {
        // Can't create a file that already exists
        if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
            errno = EEXIST;
            return NULL;
        }
    } else {
        // Create a new note
        if (!(flags & O_CREAT)) {
            errno = ENOENT;
            return NULL;
        }

        // Find an empty note
        for (note_id=0; note_id<MAX_NOTES; note_id++) {
            note = read_note(fs, note_id);
            if (!(note->status & NOTE_STATUS_OCCUPIED)) {
                break;
            }
        }

        // Too many notes
        if (note_id == MAX_NOTES) {
            errno = EMFILE;
            return NULL;
        }
    }

    int mode = flags & 7;
    cpakfs_openfile_t *file = malloc(sizeof(cpakfs_openfile_t));
    memset(file, 0, sizeof(*file));
    file->port = port;
    file->note = note;
    file->cur_page_ptr = &note->first_page;
    file->flags  = (mode == O_RDONLY || mode == O_RDWR) ? FLAG_READING : 0;
    file->flags |= (mode == O_WRONLY || mode == O_RDWR) ? FLAG_WRITING : 0;

    // If O_APPEND is set, seek to the end of the file
    if (flags & O_CREAT) {
        // Free the FAT chain for this file, unless it's a new file
        truncate_note(fs, file->note);

        // Populate the note
        memcpy((char*)note->gamecode, path.gamecode, 4);
        memcpy((char*)note->pubcode, path.pubcode, 2);
        memcpy((char*)note->filename, path.filename, 16);
        memcpy((char*)note->ext, path.ext, 4);

        note->status |= NOTE_STATUS_OCCUPIED;
        note->first_page = FAT_TERMINATOR;
        file->flags |= FLAG_NOTE_DIRTY;
    } else {
        // If O_TRUNC is set, truncate the existing file immediately
        if (flags & O_TRUNC) {
            truncate_note(fs, file->note);
            file->note->first_page = FAT_TERMINATOR;
            file->size = 0;
            file->flags |= FLAG_NOTE_DIRTY;
        } else {
            // Calculate the size only if we're not truncating.
            // This also serves as a check that the FAT chain is valid.
            file->size = calc_size(fs, note);
            if (file->size < 0) {
                errno = EFTYPE;
                free(file);
                return NULL;
            }
        }

        if (flags & O_APPEND)
            file->pos = file->size;
    }

    return file;
}

static int __cpakfs_close(void *file)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int err = 0;

    tracef("__cpak_close(%p)\n", file);

    // If we have newly allocated pages and the file doesn't end at a page boundary,
    // clean up the partial last page to avoid data leakage
    if ((f->flags & FLAG_NEW_PAGES) && (f->size % PAGE_SIZE) != 0) {
        // Navigate to the last page by following the FAT chain to the terminator
        cpakfs_fat_entry_t *last_page_ptr = &f->note->first_page;
        while (!FAT_IS_TERMINATOR(FAT_NEXT(fs->fat, *last_page_ptr))) {
            last_page_ptr = &FAT_NEXT(fs->fat, *last_page_ptr);
        }
        
        // Clear the unused portion of the last page
        int used_bytes = f->size % PAGE_SIZE;
        int cleanup_bytes = PAGE_SIZE - used_bytes;
        uint8_t zeros[PAGE_SIZE] = {0};
        if (cpak_write(f->port, last_page_ptr->bank, 
                      last_page_ptr->page * PAGE_SIZE + used_bytes, 
                      zeros, cleanup_bytes) < 0) {
            err = -1;
        }
        
        tracef("__cpak_close: cleaned %d bytes from last page\n", cleanup_bytes);
    }

    if (f->flags & FLAG_NOTE_DIRTY) {
        // Update the padding size
        f->note->ext_padding_size = (f->size % PAGE_SIZE) ? PAGE_SIZE - (f->size % PAGE_SIZE) : 0;

        // Update the crc16, so that we mark 
        f->note->ext_crc16 = 0;
        f->note->ext_crc16 = be16(crc16(f->note));

        int note_start = 0x100 + fs->fat_size*2;
        int note_id = f->note - fs->notes;
        tracef("__cpak_close: writing note %d\n", note_id);
        if (cpak_write(f->port, 0, note_start + note_id*32, f->note, 32) < 0)
            err = -1;
    }
    if (f) {
        tracef("__cpak_close: writing fat\n");
        if (write_fat(fs) < 0)
            err = -1;
    }

    free(file);
    return err;
}

static int __cpakfs_fstat(void *file, struct stat *st)
{
    cpakfs_openfile_t *f = file;

    memset(st, 0, sizeof(struct stat));
    st->st_dev = f->port;
    st->st_ino = f->note - filesystems[f->port]->notes;
    #ifndef __MINGW32__
    st->st_blksize = PAGE_SIZE;
    st->st_blocks = (f->size + PAGE_SIZE - 1) / PAGE_SIZE;
    #endif
    st->st_mode = S_IFREG;
    st->st_size = f->size;

    return 0;
}

static int __cpakfs_unlink(char *name, int port)
{
    cpakfs_path_t path;
    const char *error_pos;

    tracef("__cpak_unlink(%s, %d)\n", name, port);

    // Parse the path
    cpakfs_parse_err_t parse_result = cpakfs_path_parse(name, &path, &error_pos);
    if (parse_result != CPAKFS_PARSE_OK) {
        errno = EINVAL;
        return -1;
    }

    // Find the note for this file
    cpakfs_t *fs = filesystems[port];
    int note_id;
    cpakfs_note_t *note = find_note(fs, &path, &note_id);

    // File not found
    if (!note) {
        errno = ENOENT;
        return -1;
    }

    // Free all pages in the FAT chain
    truncate_note(fs, note);

    // Mark the note as empty
    memset(note, 0, sizeof(cpakfs_note_t));
    int note_start = 0x100 + fs->fat_size*2;
    if (cpak_write(port, 0, note_start + note_id*32, note, 32) < 0)
        return -1;

    // Write the updated FAT to disk
    if (write_fat(fs) < 0)
        return -1;

    return 0;
}

static int __cpakfs_findnext(const char *basepath, dir_t *dir, int port) {
    cpakfs_t *fs = filesystems[port];
    cpakfs_note_t *note = NULL;

    // Find the next occupied note
    while (++dir->d_cookie < MAX_NOTES) {
        note = read_note(fs, dir->d_cookie);
        
        if ((note->status & NOTE_STATUS_OCCUPIED) == 0)
            continue;

        // Found a valid file, format the name using the new dash format
        cpakfs_path_t path;
        memcpy(path.gamecode, note->gamecode, 4);
        memcpy(path.pubcode, note->pubcode, 2);
        memcpy(path.filename, note->filename, 16);
        memcpy(path.ext, note->ext, 4);

        cpakfs_path_format(&path, dir->d_name, sizeof(dir->d_name));
        dir->d_type = DT_REG;
        
        // Calculate file size. If negative, ignore the error and just return the file.
        // It means the file will fail to open, but we still want to return it.
        dir->d_size = calc_size(fs, note);

        return 0;
    }
    
    // No more files found
    errno = ENOENT;
    return -1;
}

static int __cpakfs_findfirst(char *path, dir_t *dir, int port) {
    if (!path || strcmp(path, "/") != 0) {
        errno = EINVAL;
        return -1;
    }

    dir->d_cookie = -1;
    return __cpakfs_findnext(path, dir, port);
}

static int __cpakfs_ioctl(void *file, unsigned long request, void *arg)
{
    cpakfs_openfile_t *f = (cpakfs_openfile_t*)file;
    if (!f || !f->cur_page_ptr) {
        errno = EBADF;
        return -1;
    }
    if (request == IOCPAKFS_GET_PAGE) {
        uint16_t *bankpage = (uint16_t*)arg;
        *bankpage = ((uint16_t)f->cur_page_ptr->bank << 8) | (f->cur_page_ptr->page & 0xFF);
        return 0;
    }
    errno = ENOTTY;
    return -1;
}

static void *__cpakfs_open_port0(char *name, int flags) { return __cpakfs_open(name, flags, 0); }
static void *__cpakfs_open_port1(char *name, int flags) { return __cpakfs_open(name, flags, 1); }
static void *__cpakfs_open_port2(char *name, int flags) { return __cpakfs_open(name, flags, 2); }
static void *__cpakfs_open_port3(char *name, int flags) { return __cpakfs_open(name, flags, 3); }

static int __cpakfs_findfirst_port0(char *name, dir_t *dir) { return __cpakfs_findfirst(name, dir, 0); }
static int __cpakfs_findfirst_port1(char *name, dir_t *dir) { return __cpakfs_findfirst(name, dir, 1); }
static int __cpakfs_findfirst_port2(char *name, dir_t *dir) { return __cpakfs_findfirst(name, dir, 2); }
static int __cpakfs_findfirst_port3(char *name, dir_t *dir) { return __cpakfs_findfirst(name, dir, 3); }

static int __cpakfs_findnext_port0(const char *name, dir_t *dir) { return __cpakfs_findnext(name, dir, 0); }
static int __cpakfs_findnext_port1(const char *name, dir_t *dir) { return __cpakfs_findnext(name, dir, 1); }
static int __cpakfs_findnext_port2(const char *name, dir_t *dir) { return __cpakfs_findnext(name, dir, 2); }
static int __cpakfs_findnext_port3(const char *name, dir_t *dir) { return __cpakfs_findnext(name, dir, 3); }

static int __cpakfs_unlink_port0(char *name) { return __cpakfs_unlink(name, 0); }
static int __cpakfs_unlink_port1(char *name) { return __cpakfs_unlink(name, 1); }
static int __cpakfs_unlink_port2(char *name) { return __cpakfs_unlink(name, 2); }
static int __cpakfs_unlink_port3(char *name) { return __cpakfs_unlink(name, 3); }

static filesystem_t fsdef[4] = {
    [0] = {
        .open = __cpakfs_open_port0,
        .read = __cpakfs_read,
        .close = __cpakfs_close,
        .fstat = __cpakfs_fstat,
        .write = __cpakfs_write,
        .lseek = __cpakfs_lseek,
        .unlink = __cpakfs_unlink_port0,
        .findfirst = __cpakfs_findfirst_port0,
        .findnext2 = __cpakfs_findnext_port0,
        .ioctl = __cpakfs_ioctl,
    },
    [1] = {
        .open = __cpakfs_open_port1,
        .read = __cpakfs_read,
        .close = __cpakfs_close,
        .fstat = __cpakfs_fstat,
        .write = __cpakfs_write,
        .lseek = __cpakfs_lseek,
        .unlink = __cpakfs_unlink_port1,
        .findfirst = __cpakfs_findfirst_port1,
        .findnext2 = __cpakfs_findnext_port1,
        .ioctl = __cpakfs_ioctl,
    },
    [2] = {
        .open = __cpakfs_open_port2,
        .read = __cpakfs_read,
        .close = __cpakfs_close,
        .fstat = __cpakfs_fstat,
        .write = __cpakfs_write,
        .lseek = __cpakfs_lseek,
        .unlink = __cpakfs_unlink_port2,
        .findfirst = __cpakfs_findfirst_port2,
        .findnext2 = __cpakfs_findnext_port2,
        .ioctl = __cpakfs_ioctl,
    },
    [3] = {
        .open = __cpakfs_open_port3,
        .read = __cpakfs_read,
        .close = __cpakfs_close,
        .fstat = __cpakfs_fstat,
        .write = __cpakfs_write,
        .lseek = __cpakfs_lseek,
        .unlink = __cpakfs_unlink_port3,
        .findfirst = __cpakfs_findfirst_port3,
        .findnext2 = __cpakfs_findnext_port3,
        .ioctl = __cpakfs_ioctl,
    },
};

int cpakfs_mount(joypad_port_t port, const char *prefix)
{
    joypad_accessory_type_t type = joypad_get_accessory_type(port);
    if (type != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        errno = ENODEV;
        return -1;
    }

    cpakfs_id_t fsid;
    if (fsid_read(port, &fsid) < 0)
        return -2;

    int fat_size = be16(fsid.bank_size_msb) & 0xFF00;
    cpakfs_t *fs = malloc(sizeof(cpakfs_t) + fat_size);
    memset(fs, 0, sizeof(cpakfs_t));
    fs->port = port;
    fs->fat_size = fat_size;
    fs->reserved = 1 + (fat_size >> 8) * 2 + 2; // Reserved space: sector ID, two FAT copies, note table
    tracef("cpak_mount: port %d, fs size %d\n", port, (fs->fat_size >> 8) * BANK_SIZE);

    // Force a bank switch first, as we can't know the current selected bank
    fs->cur_bank = -1;

    if (read_fat(fs) < 0) {
        free(fs);
        return -3;
    }

    if (attach_filesystem(prefix, &fsdef[port]) < 0) {
        free(fs);
        return -4;
    }

    prefixes[port] = strdup(prefix);
    filesystems[port] = fs;
    return 0;
}

int cpakfs_unmount(joypad_port_t port)
{
    cpakfs_t *fs = filesystems[port];
    if (fs == NULL) {
        errno = ENODEV;
        return -1;
    }

    if (detach_filesystem(prefixes[port]) < 0)
        return -2;

    free(fs);
    free(prefixes[port]);
    prefixes[port] = NULL;
    filesystems[port] = NULL;
    return 0;
}

int cpakfs_get_serial(joypad_port_t port, uint8_t serial[24])
{
    joypad_accessory_type_t type = joypad_get_accessory_type(port);
    if (type != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        errno = ENODEV;
        return -1;
    }

    cpakfs_id_t fsid;
    if (fsid_read(port, &fsid) < 0)
        return -2;

    memcpy(serial, fsid.serial, 24);
    return 0;
}

int cpakfs_get_stats(joypad_port_t port, cpakfs_stats_t *stats)
{
    cpakfs_t *fs = filesystems[port];
    if (fs == NULL) {
        errno = ENODEV;
        return -1;
    }

    // Reserved space: sector ID, two FAT copies, note table
    int num_banks = fs->fat_size >> 8;
    int reserved_others = num_banks - 1;

    memset(stats, 0, sizeof(*stats));
    stats->num_banks = num_banks;
    stats->notes.total = MAX_NOTES;
    stats->pages.total = num_banks * 128 - fs->reserved - reserved_others;

    for (int i=0; i<MAX_NOTES; i++) {
        cpakfs_note_t *note = read_note(fs, i);
        if (note->status & NOTE_STATUS_OCCUPIED) {
            stats->notes.used++;
        }
    }

    for (int b=0; b<num_banks; b++) {
        int first_page = (b == 0) ? fs->reserved : 1;
        for (int i=first_page; i<NUM_PAGES; i++) {
            // Count used pages. Also mark the first page of each bank as used:
            // in fact, it's an empty, wasted page that can't be allocated because
            // the relative FAT entry (entry 0 in each FAT page) is reserved for the
            // FAT page checksum.
            if (FAT_IS_VALID(fs->fat[b][i], fs->reserved) || FAT_IS_TERMINATOR(fs->fat[b][i]))
                stats->pages.used++;
        }
    }

    return 0;
}
