#include <stdio.h>
#include <stdint.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "system.h"
#include "joypad.h"
#include "joypad_accessory.h"
#include "joybus_accessory.h"
#include "utils.h"
#ifdef N64
#include "cop0.h"
#include "cpak.h"
#include "debug.h"
#endif

#define MAX_NOTES       16
#define PAGE_SIZE       256
#define BLOCK_SIZE      32

#define FAT_TERMINATOR  1
#define FAT_UNUSED      3
#define FAT_VALID(n)    ((n) >= 5)

#define NOTE_STATUS_OCCUPIED     (1<<9)
#define NOTE_STATUS_SIZE         (1<<0)     // libdragon extension

#ifndef N64
#define be16(x)   __builtin_bswap16(x)
#define be16i(x)  (int16_t)__builtin_bswap16(x)
#define be32(x)   __builtin_bswap32(x)
#define RAND()    rand()
#define debugf(...) fprintf(stderr, __VA_ARGS__)
#else
#define be16(x)   (x)
#define be16i(x)  (x)
#define be32(x)   (x)
#define RAND()    C0_COUNT()
#endif

#define FLAG_READING            (1<<0)
#define FLAG_WRITING            (1<<1)
#define FLAG_NOTE_DIRTY         (1<<2)
#define FLAG_FAT_DIRTY          (1<<3)

/** @brief ID sector */
typedef union {
    struct {
        uint8_t serial[24];         ///< Serial number
        uint16_t device_id_lsb;     ///< Device ID (in the LSB). Bit 0 should be 1
        uint16_t bank_size_msb;     ///< Bank size (in the MSB): number of FAT entries
        uint16_t checksum1;         ///< Checksum of the first 14 words
        uint16_t checksum2;         ///< Reversed checksum of the first 14 words
    };
    uint8_t data8[32];              ///< Raw data (8-bit access)
    uint16_t data16[16];            ///< Raw data (16-bit access)
} cpakfs_id_t;

/** @brief A note (similar to inode) */
typedef struct {
    char gamecode[4];               ///< Game code (ASCII, 4 chars)
    char pubcode[2];                ///< Publisher code (ASCII, 2 chars)
    uint16_t first_page;            ///< First page where data is stored
    uint16_t status;                ///< Status flags (bit 2: occupied)
    uint16_t unused;                ///< Unused
    uint8_t ext[4];                 ///< File extension (custom codepage)
    uint8_t filename[16];           ///< Filename (custom codepage)
} cpakfs_note_t;

/** @brief A mounted cpak filesystem */
typedef struct {
    joypad_port_t port;                 ///< Joypad port
    cpakfs_note_t notes[MAX_NOTES];     ///< Cache of all the notes
    uint16_t notes_mask;                ///< Bitmask of read notes
    int cur_bank;                       ///< Current bank for multi-bank cpaks
    int fat_idx;                        ///< Index of the FAT that was read (0 or 1)
    int fat_size;                       ///< Size of the FAT in bytes
    uint16_t fat[];                     ///< FAT entries
} cpakfs_t;

/** @brief A open file in the cpakfs */
typedef struct {
    int port;                           ///< Joypad port
    cpakfs_note_t *note;                ///< Note this file is associated with
    int pos;                            ///< Current position in the file
    int size;                           ///< File size
    uint16_t *cur_page_ptr;             ///< Pointer to the current page in the FAT (or in the note)
    uint8_t flags;                      ///< Flags
} cpakfs_openfile_t;

static cpakfs_t *filesystems[4];
static char *prefixes[4];

static int block_xfer(joypad_port_t port, joypad_accessory_xfer_t xfer, uint32_t addr, void *data, int nbytes)
{
    cpakfs_t *fs = filesystems[port];
    int bank = addr >> 15;
    assert((addr+nbytes-1) >> 15 == bank);  // this function only reads/writes from one bank
    
    if (fs && bank != fs->cur_bank) { // filesystem might not exist yet, in which case always switch
        if (joypad_controller_pak_set_bank(port, bank) != JOYPAD_ACCESSORY_ERROR_NONE) {
            errno = EIO;
            return -1;
        }
        fs->cur_bank = bank;
    }

    if (joypad_accessory_xfer(port, xfer, addr, data, nbytes) != JOYPAD_ACCESSORY_ERROR_NONE) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static inline int block_read(joypad_port_t port, uint32_t addr, void *data, int nbytes)
{
    return block_xfer(port, JOYPAD_ACCESSORY_XFER_READ, addr, data, nbytes);
}

static inline int block_write(joypad_port_t port, uint32_t addr, void *data, int nbytes)
{
    return block_xfer(port, JOYPAD_ACCESSORY_XFER_WRITE, addr, data, nbytes);
}

static void fsid_checksum(cpakfs_id_t *id, uint16_t *checksum1, uint16_t *checksum2)
{
    *checksum1 = 0;
    for (int i=0; i<14; i++)
        *checksum1 += be16(id->data16[i]);
    *checksum2 = 0xFFF2 - *checksum1;
}

static void fsid_new(cpakfs_id_t *id, int fat_size)
{
    assert((fat_size & (fat_size-1)) == 0);
    memcpy(id->serial, "DRAGON", 6);
    getentropy(id->serial+6, 18);
    id->device_id_lsb = be16(0x0001);
    id->bank_size_msb = be16(fat_size);

    uint16_t checksum1, checksum2;
    fsid_checksum(id, &checksum1, &checksum2);
    id->checksum1 = be16(checksum1);
    id->checksum2 = be16(checksum2);
}

static int fsid_read(joypad_port_t port, cpakfs_id_t *id)
{
    // Check if one of the ID sectors is correct
    int sectors[4] = { 0x20, 0x60, 0x80, 0xC0 };
    for (int i=0; i<4; i++) {
        if (block_read(port, sectors[i], id->data8, 32) < 0) {
            return -1;
        }

        // Check device ID
        if ((be16(id->device_id_lsb) & 0x01) != 0x01)
            continue;

        // Verify checksum
        uint16_t checksum1, checksum2;
        fsid_checksum(id, &checksum1, &checksum2);
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

static int fsid_write(joypad_port_t port, cpakfs_id_t *id)
{
    // Write ID sector
    int sectors[4] = { 0x20, 0x60, 0x80, 0xC0 };
    for (int i=0; i<4; i++) {
        if (block_write(port, sectors[i], id->data8, 32) < 0)
            return -1;
    }
    return 0;
}

static int read_fat(cpakfs_t *fs)
{
    int addr = 0x100;

    // Read FAT structure
    for (int i=0; i<2; i++) {
        if (block_read(fs->port, addr, fs->fat, fs->fat_size) < 0)
            return -1;

        // Check FAT checksum
        uint8_t checksum = 0;
        for (int i=5; i<128; i++) {
            checksum += fs->fat[i] >> 0;
            checksum += fs->fat[i] >> 8;
        }
        if (checksum == (be16(fs->fat[0]) & 0xFF)) {
            fs->fat_idx = i;
            return 0;
        }

        // Try on second copy
        addr += fs->fat_size;
    }

    // No valid FAT sector found
    errno = ENODEV;
    return -1;
}

static int write_fat(cpakfs_t *fs)
{
    // Update checksum
    uint8_t checksum = 0;
    for (int i=5; i<128; i++) {
        checksum += fs->fat[i] >> 0;
        checksum += fs->fat[i] >> 8;
    }
    fs->fat[0] = be16(checksum);

    // Write FAT structure
    int new_fat_idx = fs->fat_idx ^ 1;
    if (block_write(fs->port, 0x100 + new_fat_idx*fs->fat_size, fs->fat, fs->fat_size) < 0)
        return -1;

    // Invalidate the other FAT copy. Read the initial block, and change the checksum
    uint8_t block[32];
    if (block_read(fs->port, 0x100 + fs->fat_idx*fs->fat_size, block, 32) < 0)
        return -1;
    block[1]++;
    if (block_write(fs->port, 0x100 + fs->fat_idx*fs->fat_size, block, 32) < 0)
        return -1;

    // Remember current copy
    fs->fat_idx = new_fat_idx;
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
        case 0x00: *out++ = 0; return 1;
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

/*
 * Function that converts a UTF-8 string (input) into a string using the cpak codepage,
 * writing the output bytes into "out". The function returns the number of bytes written.
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
                /* If no corresponding mapping is found, replace with space */
                *out++ = 0x0F;
                continue;
            } else {
                /* Invalid UTF-8 sequence: replace with space */
                *out++ = 0x0F;
                input++;
                continue;
            }
        }

        /* For any other unrecognized character, replace with space */
        *out++ = 0x0F;
        input++;
    }

    return out - start_out;
}

static int __cpak_read(void *file, uint8_t *ptr, int len)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int read = 0;

    if (!(f->flags & FLAG_READING)) {
        errno = EBADF;
        return -1;
    }

    if (f->pos >= f->size)
        return 0;

    len = MIN(len, f->size - f->pos);
    while (len > 0) {
        // Perform the maximum read operation within the current page
        int page_base = be16(*f->cur_page_ptr) * PAGE_SIZE;
        int page_offset = f->pos % PAGE_SIZE;
        int n = MIN(len, PAGE_SIZE - page_offset);

        // See if we can read multiple pages at once. This is only possible if
        // they are consecutive in the filesystem.
        while (n < len && be16(fs->fat[be16(*f->cur_page_ptr)] == be16(*f->cur_page_ptr)+1)) {
            n += MIN(len-n, PAGE_SIZE);
            f->cur_page_ptr = &fs->fat[be16(*f->cur_page_ptr)];
        }

        // Perform the read
        if (block_read(f->port, page_base + page_offset, ptr, n) < 0)
            return -1;

        // Update counters and optionally move to the next page
        f->pos += n;
        ptr += n;
        len -= n;
        read += n;
        if (f->pos % PAGE_SIZE == 0) {
            f->cur_page_ptr = &fs->fat[be16(*f->cur_page_ptr)];
        }
    }
    return read;
}

static int allocate_page(cpakfs_t *fs)
{
    // Search for a free page, starting from a random position
    assert((fs->fat_size & (fs->fat_size-1)) == 0);
    int page = RAND() & (fs->fat_size/2-1);

    for (int i=0; i<fs->fat_size/2; i++) {
        if (be16(fs->fat[page]) == FAT_UNUSED) {
            fs->fat[page] = be16(FAT_TERMINATOR);
            return page;
        }
        page = (page + 1) & (fs->fat_size/2-1);
    }
    errno = ENOSPC;
    return -1;
}

static int __cpak_write(void *file, uint8_t *ptr, int len)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int written = 0;

    if (!(f->flags & FLAG_WRITING)) {
        errno = EBADF;
        return -1;
    }

    while (len > 0) {
        int page_offset = f->pos % PAGE_SIZE;
        int n = MIN(len, PAGE_SIZE - page_offset);

        // Allocate a new page if necessary
        if (page_offset == 0 && !FAT_VALID(be16(*f->cur_page_ptr))) {
            int new_page = allocate_page(fs);
            if (new_page < 0)
                return -1;
            *f->cur_page_ptr = be16(new_page);
            assert(FAT_VALID(be16(*f->cur_page_ptr)));
            f->flags |= FLAG_FAT_DIRTY;
        }

        int page_base = be16(*f->cur_page_ptr) * PAGE_SIZE;
        if (block_write(f->port, page_base + page_offset, ptr, n) < 0)
            return -1;

        f->pos += n;
        ptr += n;
        len -= n;
        written += n;

        if (f->pos % PAGE_SIZE == 0) {
            assert(FAT_VALID(be16(*f->cur_page_ptr)));
            f->cur_page_ptr = &fs->fat[be16(*f->cur_page_ptr)];
        }
    }

    // If this write increased the file size, record the new size
    if (f->pos > f->size) {
        f->size = f->pos;
        f->flags |= FLAG_NOTE_DIRTY;
    }

    return written;
}

static int __cpak_lseek(void *file, int offset, int whence)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int size = f->size;
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

    // Check if the current page changed. If so, update the current page.
    bool page_changed = pos / PAGE_SIZE != f->pos / PAGE_SIZE;
    if (page_changed) {
        int page_idx = pos / PAGE_SIZE;
        f->cur_page_ptr = &f->note->first_page;
        for (int i=1; i<page_idx; i++) {
            if (!FAT_VALID(be16(*f->cur_page_ptr))) {
                errno = EFTYPE;
                return -1;
            }
            f->cur_page_ptr = &fs->fat[be16(*f->cur_page_ptr)];
        }
    }

    f->pos = pos;
    return pos;
}

static cpakfs_note_t* read_note(cpakfs_t *fs, int note_id)
{
    assert(note_id >= 0 && note_id < MAX_NOTES);
    cpakfs_note_t *note = &fs->notes[note_id];
    if (!(fs->notes_mask & (1 << note_id))) {
        int note_start = 0x100 + fs->fat_size*2;
        if (block_read(fs->port, note_start + note_id*32, note, sizeof(cpakfs_note_t)) < 0)
            return NULL;
    }
    return note;
}

static int calc_size(cpakfs_t *fs, int first_page)
{
    int size = 0;
    int cur_page = first_page;
    while (cur_page != FAT_TERMINATOR) {
        if (!FAT_VALID(cur_page) || size > PAGE_SIZE * fs->fat_size) { // prevent infinite loop
            errno = EFTYPE;
            return -1;
        }
        size += PAGE_SIZE;
        cur_page = be16(fs->fat[cur_page]);
    }
    return size;
}


static void *__cpak_open(char *name, int flags, int port)
{
    char filename[16+1]={0}, ext[4+1]={0};
    printf("opening %s\n", name);

    // Check the format is "GAME.PB/..."
    if (strlen(name) < 9 || name[4] != '.' || name[7] != '/') {
        errno = EINVAL;
        return NULL;
    }

    // Extract filename and convert to cpak codepage format
    char *fname = name + 8;
    char *dot = strrchr(fname, '.');
    int fnlen = utf8_to_n64(fname, dot ? dot-fname : strlen(fname), (uint8_t*)filename, 16);
    filename[fnlen] = 0;

    // Extract extension and convert to cpak codepage format
    if (dot) {
        dot++;
        int extlen = utf8_to_n64(dot, strlen(dot), (uint8_t*)ext, 4);
        ext[extlen] = 0;
    }

    // Go through the notes and see if we can find a match.
    cpakfs_t *fs = filesystems[port];
    cpakfs_note_t *note = NULL;
    int note_id = MAX_NOTES;
    for (note_id=0; note_id<MAX_NOTES; note_id++) {
        note = read_note(fs, note_id);
        if (!(be16(note->status) & NOTE_STATUS_OCCUPIED))
            continue;

        if (memcmp(note->gamecode, name+0, 4) == 0 &&
            memcmp(note->pubcode,  name+5, 2) == 0 &&
            strncmp((char*)note->filename, filename, 16) == 0 &&
            strncmp((char*)note->ext, ext, 4) == 0)
            break;
    }

    if (note_id < MAX_NOTES) {
        // Can't create a file that already exists
        printf("opening note %d\n", note_id);
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
            if (!(be16(note->status) & NOTE_STATUS_OCCUPIED)) {
                printf("creating note %d\n", note_id);
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
        file->size = 0;

        // Free the FAT chain for this file, unless it's a new file
        uint16_t *prevpage = &file->note->first_page;
        while (FAT_VALID(be16(*prevpage)) || be16(*prevpage) == FAT_TERMINATOR) {
            uint16_t page = be16(*prevpage);
            *prevpage = be16(FAT_UNUSED);
            prevpage = &fs->fat[page];
        }

        memcpy((char*)note->gamecode, name+0, 4);
        memcpy((char*)note->pubcode, name+5, 2);
        memcpy((char*)note->filename, filename, 16);
        memcpy((char*)note->ext, ext, 4);

        note->status |= be16(NOTE_STATUS_OCCUPIED);
        note->first_page = be16(FAT_UNUSED);
        file->flags |= FLAG_NOTE_DIRTY | FLAG_FAT_DIRTY;
    } else {
        // Calculate the size
        file->size = calc_size(fs, be16(note->first_page));
        if (file->size < 0) {
            free(file);
            return NULL;
        }

        if (flags & O_APPEND)
            file->pos = file->size;
    }

    return file;
}

static int __cpak_close(void *file)
{
    cpakfs_openfile_t *f = file;
    cpakfs_t *fs = filesystems[f->port];
    int err = 0;

    if (f->flags & FLAG_NOTE_DIRTY) {
        int note_start = 0x100 + fs->fat_size*2;
        int note_id = f->note - fs->notes;
        if (block_write(f->port, note_start + note_id*32, f->note, 32) < 0)
            err = -1;
    }
    if (f->flags & FLAG_FAT_DIRTY) {
        if (write_fat(fs) < 0)
            err = -1;
    }

    free(file);
    return err;
}

static int __cpak_fstat(void *file, struct stat *st)
{
    cpakfs_openfile_t *f = file;

    memset(st, 0, sizeof(struct stat));
    st->st_dev = f->port;
    st->st_ino = f->note - filesystems[f->port]->notes;
    st->st_blksize = PAGE_SIZE;
    st->st_blocks = (f->size + PAGE_SIZE - 1) / PAGE_SIZE;
    st->st_mode = S_IFREG;
    st->st_size = f->size;

    return 0;
}

static int __cpak_findnext(dir_t *dir, int port) {
    cpakfs_t *fs = filesystems[port];
    cpakfs_note_t *note = NULL;

    if ((int)dir->d_cookie < MAX_NOTES) {
        note = read_note(fs, ++dir->d_cookie);
        while ((int)dir->d_cookie < MAX_NOTES-1 && (be16(note->status) & NOTE_STATUS_OCCUPIED) == 0) {
            note = read_note(fs, ++dir->d_cookie);
        }
    }
    if ((int)dir->d_cookie == MAX_NOTES) {
        errno = ENOENT;
        return -1;
    }

    snprintf(dir->d_name, sizeof(dir->d_name), "%.4s.%.2s/", note->gamecode, note->pubcode);
    int idx = 8;
    for (int i=0; i<16; i++) {
        if (note->filename[i] == 0)
            break;
        idx += n64_to_utf8(note->filename[i], dir->d_name + idx);
    }
    if (note->ext[0] != 0) {
        dir->d_name[idx++] = '.';
        for (int i=0; i<4; i++) {
            if (note->ext[i] == 0)
                break;
            idx += n64_to_utf8(note->ext[i], dir->d_name + idx);
        }
    }
    dir->d_name[idx] = 0;
    dir->d_type = DT_REG;
    
    // Calculate file size
    dir->d_size = calc_size(fs, be16(note->first_page));
    if (dir->d_size < 0) {
        errno = EFTYPE;
        return -1;
    }

    return 0;
}

static int __cpak_findfirst(char *path, dir_t *dir, int port) {
    if (!path || strcmp(path, "/")) {
        errno = EINVAL;
        return -2;
    }

    dir->d_cookie = -1;
    return __cpak_findnext(dir, port);
}


static void *__cpak_open_port0(char *name, int flags) { return __cpak_open(name, flags, 0); }
static void *__cpak_open_port1(char *name, int flags) { return __cpak_open(name, flags, 1); }
static void *__cpak_open_port2(char *name, int flags) { return __cpak_open(name, flags, 2); }
static void *__cpak_open_port3(char *name, int flags) { return __cpak_open(name, flags, 3); }

static int __cpak_findfirst_port0(char *name, dir_t *dir) { return __cpak_findfirst(name, dir, 0); }
static int __cpak_findfirst_port1(char *name, dir_t *dir) { return __cpak_findfirst(name, dir, 1); }
static int __cpak_findfirst_port2(char *name, dir_t *dir) { return __cpak_findfirst(name, dir, 2); }
static int __cpak_findfirst_port3(char *name, dir_t *dir) { return __cpak_findfirst(name, dir, 3); }

static int __cpak_findnext_port0(dir_t *dir) { return __cpak_findnext(dir, 0); }
static int __cpak_findnext_port1(dir_t *dir) { return __cpak_findnext(dir, 1); }
static int __cpak_findnext_port2(dir_t *dir) { return __cpak_findnext(dir, 2); }
static int __cpak_findnext_port3(dir_t *dir) { return __cpak_findnext(dir, 3); }

static filesystem_t fsdef[4] = {
    [0] = {
        .open = __cpak_open_port0,
        .read = __cpak_read,
        .close = __cpak_close,
        .fstat = __cpak_fstat,
        .write = __cpak_write,
        .lseek = __cpak_lseek,
        .findfirst = __cpak_findfirst_port0,
        .findnext = __cpak_findnext_port0,
    },
    [1] = {
        .open = __cpak_open_port1,
        .read = __cpak_read,
        .close = __cpak_close,
        .fstat = __cpak_fstat,
        .write = __cpak_write,
        .lseek = __cpak_lseek,
        .findfirst = __cpak_findfirst_port1,
        .findnext = __cpak_findnext_port1,
    },
    [2] = {
        .open = __cpak_open_port2,
        .read = __cpak_read,
        .close = __cpak_close,
        .fstat = __cpak_fstat,
        .write = __cpak_write,
        .lseek = __cpak_lseek,
        .findfirst = __cpak_findfirst_port2,
        .findnext = __cpak_findnext_port2,
    },
    [3] = {
        .open = __cpak_open_port3,
        .read = __cpak_read,
        .close = __cpak_close,
        .fstat = __cpak_fstat,
        .write = __cpak_write,
        .lseek = __cpak_lseek,
        .findfirst = __cpak_findfirst_port3,
        .findnext = __cpak_findnext_port3,
    },
};

int cpak_mount(joypad_port_t port, const char *prefix)
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

int cpak_unmount(joypad_port_t port)
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

int cpak_get_serial(joypad_port_t port, uint8_t serial[24])
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

int cpak_get_stats(joypad_port_t port, cpak_stats_t *stats)
{
    cpakfs_t *fs = filesystems[port];
    if (fs == NULL) {
        errno = ENODEV;
        return -1;
    }

    memset(stats, 0, sizeof(*stats));
    stats->notes.total = MAX_NOTES;
    stats->pages.total = fs->fat_size;
    stats->pages.used = 5; // first 5 pages are reserved

    for (int i=0; i<MAX_NOTES; i++) {
        cpakfs_note_t *note = read_note(fs, i);
        if (be16(note->status) & NOTE_STATUS_OCCUPIED) {
            stats->notes.used++;
        }
    }

    for (int i=0; i<fs->fat_size - 5; i++) {
        if (FAT_VALID(be16(fs->fat[i]))) {
            stats->pages.used++;
        }
    }

    return 0;
}

static int probe_size(joypad_port_t port)
{
    return 0x100;
}

int cpak_fsck(joypad_port_t port, bool fix_errors)
{
    joypad_accessory_type_t type = joypad_get_accessory_type(port);
    if (type != JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        errno = ENODEV;
        return -1;
    }

    cpakfs_id_t fsid;
    if (fsid_read(port, &fsid) < 0) {
        debugf("fsck: failed to read sector ID\n");
        if (fix_errors) {
            debugf("fsck: generating new sector ID\n");
            int fat_size = probe_size(port);
            fsid_new(&fsid, be16(fsid.bank_size_msb) & 0xFF00);
            if (fsid_write(port, &fsid) < 0) {
                return -2;
            }
        }
    }

    return 0;
}

