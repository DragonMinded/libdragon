/**
 * @file cpakfs.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak Filesystem Routines
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

/** @brief Parsed cpakfs path components */
typedef struct {
    char gamecode[4];                   ///< Game code (4 chars)
    char pubcode[2];                    ///< Publisher code (2 chars) 
    char filename[16+1];                ///< Filename in N64 codepage (null-terminated)
    char ext[4+1];                      ///< Extension in N64 codepage (null-terminated)
} parsed_path_t;

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

static int n64_to_utf8(uint8_t c, char *out)
{
    /* Upper case letters */
    if (c >= 0x1A && c <= 0x33) { *out++ = 'A' + (c - 0x1A); return 1; }
    /* Numbers */
    if (c >= 0x10 && c <= 0x19) { *out++ = '0' + (c - 0x10); return 1; }
    /* Miscelaneous chart */
    switch (c) {
        case 0x00: *out++ = 0x01; return 1; // we can't store 0x00 in a ASCIIZ C string, so use 0x01 instead
        case 0x0F: *out++ = ' '; return 1;
        case 0x34: *out++ = '!'; return 1;
        case 0x35: *out++ = '\"'; return 1;
        case 0x36: *out++ = '#'; return 1;
        case 0x37: *out++ = '`'; return 1;
        case 0x38: *out++ = '*'; return 1;
        case 0x39: *out++ = '+'; return 1;
        case 0x3A: *out++ = ','; return 1;
        case 0x3B: *out++ = '-'; return 1;
        case 0x3C: *out++ = '.'; return 1;
        case 0x3D: *out++ = '/'; return 1;
        case 0x3E: *out++ = ':'; return 1;
        case 0x3F: *out++ = '='; return 1;
        case 0x40: *out++ = '?'; return 1;
        case 0x41: *out++ = '@'; return 1;
    }

    /* Katakana and CJK symbols */
    if (c >= 0x42 && c <= 0x94) {
        const int cjk_base = 0x3000;
        static uint8_t cjk_map[83] = { 2, 155, 156, 161, 163, 165, 167, 169, 195, 227, 229, 231, 242, 243, 162, 164, 166, 168, 170, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 196, 198, 200, 202, 203, 204, 205, 206, 207, 210, 213, 216, 219, 222, 223, 224, 225, 226, 228, 230, 232, 233, 234, 235, 236, 237, 239, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, 194, 197, 199, 201, 208, 211, 214, 217, 220, 209, 212, 215, 218, 221 };
        uint16_t codepoint = cjk_base + cjk_map[c - 0x42];
        *out++ = 0xE0 | ((codepoint >> 12) & 0x0F);
        *out++ = 0x80 | ((codepoint >> 6) & 0x3F);
        *out++ = 0x80 | (codepoint & 0x3F);
        return 3;
    }

    /* Default to space for unprintables */
    *out++ = ' ';
    return 1;
}

/**
 * @brief Convert an N64 codepage string to UTF-8, handling embedded NULs correctly
 * 
 * This function converts a fixed-size N64 codepage string to UTF-8. It treats
 * only trailing 0x00 bytes as padding, while embedded 0x00 bytes are converted
 * to 0x01 in the UTF-8 output.
 * 
 * @param n64_str   Input N64 codepage string (fixed size array)
 * @param n64_len   Length of the input array
 * @param utf8_out  Output UTF-8 string buffer
 * @return int      Number of UTF-8 bytes written, or -1 on error
 */
static int n64_string_to_utf8(const uint8_t *n64_str, int n64_len, char *utf8_out)
{
    // Find the actual string length by scanning backwards to find last non-zero
    int actual_len = n64_len;
    while (actual_len > 0 && n64_str[actual_len - 1] == 0x00) {
        actual_len--;
    }
    
    int utf8_pos = 0;
    for (int i = 0; i < actual_len; i++) {
        int written = n64_to_utf8(n64_str[i], utf8_out + utf8_pos);
        utf8_pos += written;
    }
    
    utf8_out[utf8_pos] = '\0';
    return utf8_pos;
}

/**
 * @brief Check if an N64 codepage string has any non-padding content
 * 
 * @param n64_str   Input N64 codepage string (fixed size array)
 * @param n64_len   Length of the input array
 * @return bool     True if the string has content, false if it's all padding (0x00)
 */
static bool n64_string_has_content(const uint8_t *n64_str, int n64_len)
{
    for (int i = 0; i < n64_len; i++) {
        if (n64_str[i] != 0x00) {
            return true;
        }
    }
    return false;
}

/*
 * Function that converts a UTF-8 string (input) into a string using the cpak codepage,
 * writing the output bytes into "out". The function returns the number of bytes written.
 * 
 * The function will fail if any unsupported character is encountered, returning -1.
 */
static int utf8_to_n64(const char *input, int in_size, uint8_t *out, int out_size) {
    const char *end_input = input + in_size;
    const uint8_t *start_out = out;
    const uint8_t *end_out = out + out_size;

    /* 
     * Inverse lookup table for the CJK part.
     * For every possible offset (codepoint – 0x3000) from 0 to 255,
     * the value is the index in the range [0, 82] to be added to 0x42.
     * If an offset does not correspond to any mapped character, the value will be -1.
     */
    static const int8_t inv_cjk_map[256] = {
        [2]   = 1, [155] = 2, [156] = 3, [161] = 4,
        [163] = 5, [165] = 6, [167] = 7, [169] = 8, [195] = 9, 
        [227] = 10, [229] = 11, [231] = 12, [242] = 13, [243] = 14, 
        [162] = 15, [164] = 16, [166] = 17, [168] = 18,
        [170] = 19, [171] = 20, [173] = 21, [175] = 22,
        [177] = 23, [179] = 24, [181] = 25, [183] = 26,
        [185] = 27, [187] = 28, [189] = 29, [191] = 30,
        [193] = 31, [196] = 32, [198] = 33, [200] = 34,
        [202] = 35, [203] = 36, [204] = 37, [205] = 38,
        [206] = 39, [207] = 40, [210] = 41, [213] = 42,
        [216] = 43, [219] = 44, [222] = 45, [223] = 46,
        [224] = 47, [225] = 48, [226] = 49, [228] = 50,
        [230] = 51, [232] = 52, [233] = 53, [234] = 54,
        [235] = 55, [236] = 56, [237] = 57, [239] = 58,
        [172] = 59, [174] = 60, [176] = 61, [178] = 62,
        [180] = 63, [182] = 64, [184] = 65, [186] = 66,
        [188] = 67, [190] = 68, [192] = 69, [194] = 70,
        [197] = 71, [199] = 72, [201] = 73, [208] = 74,
        [211] = 75, [214] = 76, [217] = 77, [220] = 78,
        [209] = 79, [212] = 80, [215] = 81, [218] = 82,
        [221] = 83,
    };

    /*
     * Conversion loop: for each character (or UTF-8 sequence)
     * we search for the corresponding byte in the n64 codepage.
     */
    while (*input && input < end_input && out < end_out) {
        unsigned char ch = (unsigned char)*input;

        /* Uppercase letters: 'A'-'Z' */
        if (ch >= 'A' && ch <= 'Z') {
            *out++ = 0x1A + (ch - 'A');
            input++;
            continue;
        }

        /* Lowercase letters: 'a'-'z' - convert to uppercase */
        if (ch >= 'a' && ch <= 'z') {
            *out++ = 0x1A + (ch - 'a');
            input++;
            continue;
        }

        /* Numbers: '0'-'9' */
        if (ch >= '0' && ch <= '9') {
            *out++ = 0x10 + (ch - '0');
            input++;
            continue;
        }

        /* Direct handling of symbols (corresponds to the "miscellaneous" part of the n64->utf8 function) */
        if (ch < 0x80) switch (ch) {
            case ' ': *out++ = 0x0F; input++; continue;
            case '!': *out++ = 0x34; input++; continue;
            case '"': *out++ = 0x35; input++; continue;
            case '#': *out++ = 0x36; input++; continue;
            case '`': *out++ = 0x37; input++; continue;
            case '*': *out++ = 0x38; input++; continue;
            case '+': *out++ = 0x39; input++; continue;
            case ',': *out++ = 0x3A; input++; continue;
            case '-': *out++ = 0x3B; input++; continue;
            case '.': *out++ = 0x3C; input++; continue;
            case '/': *out++ = 0x3D; input++; continue;
            case ':': *out++ = 0x3E; input++; continue;
            case '=': *out++ = 0x3F; input++; continue;
            case '?': *out++ = 0x40; input++; continue;
            case '@': *out++ = 0x41; input++; continue;
            case 0x01: *out++ = 0x00; input++; continue; // special case for 0x01
        }

        /* Handling of CJK characters (3-byte UTF-8 sequences) */
        if (ch >= 0xE0) {
            if ((input[0] & 0xF0) == 0xE0 &&
                (input[1] & 0xC0) == 0x80 &&
                (input[2] & 0xC0) == 0x80)
            {
                /* Decode the codepoint */
                uint16_t codepoint = ((input[0] & 0x0F) << 12) |
                                     ((input[1] & 0x3F) << 6)  |
                                     (input[2] & 0x3F);
                input += 3;

                /* If the codepoint is in the expected range for CJK conversion (based on 0x3000) */
                if (codepoint >= 0x3000) {
                    uint16_t offset = codepoint - 0x3000;
                    if (offset < 256) {
                        int8_t index = inv_cjk_map[offset];
                        if (index > 0) {
                            *out++ = 0x42 + index - 1;
                            continue;
                        }
                    }
                }
                return -1;
            } else {
                return -1;
            }
        }
        return -1;
    }

    return out - start_out;
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
 * @param parsed    Parsed path components to search for
 * @param note_id   Output pointer to store the found note ID
 * @return cpakfs_note_t* Pointer to the found note, or NULL if not found
 */
static cpakfs_note_t* find_note(cpakfs_t *fs, const parsed_path_t *parsed, int *note_id)
{
    cpakfs_note_t *note = NULL;
    
    for (int i = 0; i < MAX_NOTES; i++) {
        note = read_note(fs, i);
        if (!(note->status & NOTE_STATUS_OCCUPIED))
            continue;

        if (memcmp(note->gamecode, parsed->gamecode, 4) == 0 &&
            memcmp(note->pubcode, parsed->pubcode, 2) == 0 &&
            memcmp(note->filename, parsed->filename, 16) == 0 &&
            memcmp(note->ext, parsed->ext, 4) == 0) {
            
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

/**
 * @brief Parse a cpakfs path into its components
 * 
 * Parses a path in the format "GAME.PB/filename.ext" and extracts the
 * game code, publisher code, filename, and extension.
 * 
 * @param name      Input path to parse
 * @param parsed    Output structure to fill with parsed components
 * @return int      0 on success, -1 on error (errno will be set)
 */
static int parse_path(const char *name, parsed_path_t *parsed)
{
    // Clear the output structure
    memset(parsed, 0, sizeof(*parsed));

    // Check the format is "GAME.PB/..."
    if (strlen(name) < 9 || name[4] != '.' || name[7] != '/') {
        errno = EINVAL;
        return -1;
    }

    // Extract gamecode and pubcode
    memcpy(parsed->gamecode, name + 0, 4);
    memcpy(parsed->pubcode, name + 5, 2);

    // Extract filename and extension from path
    char *fname = (char*)name + 8;
    char *dot = strrchr(fname, '.');
    
    // Parse filename and extension
    int fname_len = dot ? dot - fname : strlen(fname);
    if (fname_len == 0 || fname_len > 16) {
        errno = EINVAL;
        return -1;
    }
    
    int fnlen = utf8_to_n64(fname, fname_len, (uint8_t*)parsed->filename, 16);
    if (fnlen < 0) {
        errno = EINVAL;
        return -1;
    }

    // Extract and validate extension
    if (dot) {
        dot++;
        int ext_len = strlen(dot);
        
        // Validate extension length and characters
        if (ext_len > 4) {
            errno = EINVAL;
            return -1;
        }
        int extlen = utf8_to_n64(dot, ext_len, (uint8_t*)parsed->ext, 4);
        if (extlen < 0) {
            errno = EINVAL;
            return -1;
        }
    }

    return 0;
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
        for (int i=1; i<page_idx; i++) {
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
    parsed_path_t parsed;

    tracef("__cpak_open(%s, %d, %d) %d\n", name, flags, port, errno);

    // Parse the path
    if (parse_path(name, &parsed) < 0)
        return NULL;

    // Find the note for this file
    cpakfs_t *fs = filesystems[port];
    int note_id;
    cpakfs_note_t *note = find_note(fs, &parsed, &note_id);

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
            errno = ENOSPC;
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
        memcpy((char*)note->gamecode, parsed.gamecode, 4);
        memcpy((char*)note->pubcode, parsed.pubcode, 2);
        memcpy((char*)note->filename, parsed.filename, 16);
        memcpy((char*)note->ext, parsed.ext, 4);

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
    parsed_path_t parsed;

    tracef("__cpak_unlink(%s, %d)\n", name, port);

    // Parse the path
    if (parse_path(name, &parsed) < 0)
        return -1;

    // Find the note for this file
    cpakfs_t *fs = filesystems[port];
    int note_id;
    cpakfs_note_t *note = find_note(fs, &parsed, &note_id);

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

    // tracef("__cpak_findnext(%s, %p, %d) (cookie: %d)\n", basepath, dir, port, (int)dir->d_cookie);
    while ((int)dir->d_cookie < MAX_NOTES) {
        note = read_note(fs, dir->d_cookie++);
        
        if ((note->status & NOTE_STATUS_OCCUPIED) == 0)
            continue;
        
        snprintf(dir->d_name, sizeof(dir->d_name), "%.4s.%.2s/", note->gamecode, note->pubcode);
        if (basepath && strncmp(basepath+1, dir->d_name, strlen(basepath)-1) != 0)
            continue;

        // Valid filename, optionally matching the basepath: stop searching
        break;
    }
    if ((int)dir->d_cookie == MAX_NOTES) {
        errno = ENOENT;
        return -1;
    }

    int idx = 8; // Start after "GAME.PB/"
    idx += n64_string_to_utf8(note->filename, 16, dir->d_name + idx);
    if (n64_string_has_content(note->ext, 4)) {
        if (idx < sizeof(dir->d_name) - 1) {
            dir->d_name[idx++] = '.';
        }
        idx += n64_string_to_utf8(note->ext, 4, dir->d_name + idx);
    }
    dir->d_name[idx++] = 0;
    assert(idx < sizeof(dir->d_name));
    dir->d_type = DT_REG;
    
    // Calculate file size. If negative, ignore the error and just return the file.
    // It means the file will fail to open, but we still want to return it.
    dir->d_size = calc_size(fs, note);

    return 0;
}

static int __cpakfs_findfirst(char *path, dir_t *dir, int port) {
    if (!path) {
        errno = EINVAL;
        return -2;
    }

    dir->d_cookie = 0;
    return __cpakfs_findnext(path, dir, port);
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
    if (!fsid_read(port, &fsid))
        return false;

    memcpy(serial, fsid.serial, 24);
    return true;
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
