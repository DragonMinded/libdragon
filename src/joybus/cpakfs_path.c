/**
 * @file cpakfs_path.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak Filesystem Routines
 * @ingroup controllerpak
 */
#include "cpakfs.h"
#include "cpakfs_internal.h"
#include <limits.h>

/**
 * @brief Check if a character is valid for gamecode/pubcode
 */
static bool is_valid_gamecode_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/**
 * @brief Check if a character is a valid hex digit
 */
static bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

/**
 * @brief Convert a hex character to its numeric value
 */
static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/**
 * @brief Parse a hex string into bytes
 */
static bool parse_hex_string(const char *hex_str, int hex_len, uint8_t *output, int output_len)
{
    if (hex_len != output_len * 2) return false;
    
    for (int i = 0; i < output_len; i++) {
        if (!is_hex_char(hex_str[i*2]) || !is_hex_char(hex_str[i*2+1])) {
            return false;
        }
        output[i] = (hex_to_int(hex_str[i*2]) << 4) | hex_to_int(hex_str[i*2+1]);
    }
    return true;
}

/**
 * @brief Calculate the actual length of a N64 codepage string (excluding zero padding)
 */
static int n64_string_length(const uint8_t *str, int max_len)
{
    int len = max_len;
    while (len > 0 && str[len - 1] == 0x00) {
        len--;
    }
    return len;
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

/*
 * Function that converts a UTF-8 string (input) into a string using the cpak codepage,
 * writing the output bytes into "out". The function returns the number of bytes written.
 * 
 * The function will fail if any unsupported character is encountered, returning -1.
 * If the output buffer is too small, returns -2 and sets *error_input to the position
 * in the input string where conversion stopped.
 */
static int utf8_to_n64(const char *input, int in_size, uint8_t *out, int out_size, const char **error_input) {
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
    while (*input && input < end_input) {
        // Check if we have space in output buffer
        if (out >= end_out) {
            if (error_input) *error_input = input;
            return -2; // Buffer overflow
        }
        
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
                if (error_input) *error_input = input - 3;
                return -1;
            } else {
                if (error_input) *error_input = input;
                return -1;
            }
        }
        if (error_input) *error_input = input;
        return -1;
    }

    return out - start_out;
}

cpakfs_parse_err_t cpakfs_path_parse(const char *utf8_fullname, cpakfs_path_t *path, const char **error_pos)
{
    if (!utf8_fullname || !path) {
        if (error_pos) *error_pos = utf8_fullname;
        return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
    }

    // Clear the output structure
    memset(path, 0, sizeof(*path));

    const char *p = utf8_fullname;
    
    // Parse game code (4 characters normal, or 8 characters hex)
    if (strlen(p) < 4) {
        // Too short - point to the end of string or the premature separator
        const char *dot_pos = strchr(p, '.');
        if (error_pos) *error_pos = dot_pos ? dot_pos : (p + strlen(p));
        return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
    }
    
    if (strlen(p) >= 4 && p[4] == '.') {
        // 4-character alphanumeric format
        for (int i = 0; i < 4; i++) {
            if (!is_valid_gamecode_char(p[i])) {
                if (error_pos) *error_pos = &p[i];
                return CPAKFS_PARSE_ERR_GAMECODE_CHAR;
            }
            // Convert lowercase to uppercase for gamecode
            path->gamecode[i] = (p[i] >= 'a' && p[i] <= 'z') ? p[i] - 'a' + 'A' : p[i];
        }
        p += 4;
    } else if (strlen(p) >= 8 && p[8] == '.') {
        // Check if all 8 characters are valid hex before attempting hex parsing
        bool all_hex = true;
        for (int i = 0; i < 8; i++) {
            if (!is_hex_char(p[i])) {
                all_hex = false;
                break;
            }
        }
        
        if (all_hex) {
            // 8-character hex format
            if (!parse_hex_string(p, 8, path->gamecode, 4)) {
                if (error_pos) *error_pos = p;
                return CPAKFS_PARSE_ERR_GAMECODE_CHAR;  // Invalid hex = character error
            }
            p += 8;
        } else {
            // 8 characters but not all hex - this is too long for normal format
            // Point to the 5th character (first one that makes it too long for normal format)
            if (error_pos) *error_pos = p + 4;
            return CPAKFS_PARSE_ERR_GAMECODE_TOO_LONG;
        }
    } else {
        // Check length to determine if it's too short or too long
        const char *dot_pos = strchr(p, '.');
        int len = dot_pos ? (dot_pos - p) : strlen(p);
        
        if (len < 4) {
            // Too short - point to the separator that terminates prematurely
            if (error_pos) *error_pos = dot_pos ? dot_pos : (p + strlen(p));
            return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
        } else {
            // len >= 5, which is too long for normal format (4 chars) and not valid hex format
            // Point to the 5th character (first one that makes it too long)
            if (error_pos) *error_pos = p + 4;
            return CPAKFS_PARSE_ERR_GAMECODE_TOO_LONG;
        }
    }

    // Expect dot separator
    if (*p != '.') {
        if (error_pos) *error_pos = p;
        return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
    }
    p++;

    // Parse publisher code (2 characters normal, or 4 characters hex)
    if (strlen(p) < 2) {
        // Too short - point to the end of string or the premature separator
        const char *dash_pos = strchr(p, '-');
        if (error_pos) *error_pos = dash_pos ? dash_pos : (p + strlen(p));
        return CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT;
    }
    
    if (strlen(p) >= 2 && p[2] == '-') {
        // 2-character alphanumeric format
        for (int i = 0; i < 2; i++) {
            if (!is_valid_gamecode_char(p[i])) {
                if (error_pos) *error_pos = &p[i];
                return CPAKFS_PARSE_ERR_PUBCODE_CHAR;
            }
            // Convert lowercase to uppercase for pubcode
            path->pubcode[i] = (p[i] >= 'a' && p[i] <= 'z') ? p[i] - 'a' + 'A' : p[i];
        }
        p += 2;
    } else if (strlen(p) >= 4 && p[4] == '-') {
        // Check if all 4 characters are valid hex before attempting hex parsing
        bool all_hex = true;
        for (int i = 0; i < 4; i++) {
            if (!is_hex_char(p[i])) {
                all_hex = false;
                break;
            }
        }
        
        if (all_hex) {
            // 4-character hex format
            if (!parse_hex_string(p, 4, path->pubcode, 2)) {
                if (error_pos) *error_pos = p;
                return CPAKFS_PARSE_ERR_PUBCODE_CHAR;  // Invalid hex = character error
            }
            p += 4;
        } else {
            // 4 characters but not all hex - this is too long for normal format
            // Point to the 3rd character (first one that makes it too long for normal format)
            if (error_pos) *error_pos = p + 2;
            return CPAKFS_PARSE_ERR_PUBCODE_TOO_LONG;
        }
    } else {
        // Check length to determine if it's too short or too long
        const char *dash_pos = strchr(p, '-');
        int len = dash_pos ? (dash_pos - p) : strlen(p);
        
        if (len < 2) {
            // Too short - point to the separator that terminates prematurely
            if (error_pos) *error_pos = dash_pos ? dash_pos : (p + strlen(p));
            return CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT;
        } else {
            // len >= 3, which is too long for normal format (2 chars) and not valid hex format
            // Point to the 3rd character (first one that makes it too long)
            if (error_pos) *error_pos = p + 2;
            return CPAKFS_PARSE_ERR_PUBCODE_TOO_LONG;
        }
    }

    // Expect dash separator
    if (*p != '-') {
        if (error_pos) *error_pos = p;
        return CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT;
    }
    p++;

    // Parse filename (up to last dot or end of string)
    const char *filename_start = p;
    const char *filename_end = strrchr(p, '.');  // Use strrchr to find LAST dot
    if (!filename_end) {
        filename_end = p + strlen(p);
    }
    
    int filename_utf8_len = filename_end - filename_start;
    if (filename_utf8_len == 0) {
        if (error_pos) *error_pos = filename_start;
        return CPAKFS_PARSE_ERR_FILENAME_TOO_SHORT;
    }
    
    // Convert filename from UTF-8 to N64 codepage
    const char *filename_error_pos;
    int n64_filename_len = utf8_to_n64(filename_start, filename_utf8_len, path->filename, 16, &filename_error_pos);
    if (n64_filename_len < 0) {
        if (n64_filename_len == -2) {
            // Buffer overflow - filename too long
            if (error_pos) *error_pos = filename_error_pos;
            return CPAKFS_PARSE_ERR_FILENAME_TOO_LONG;
        } else {
            // Invalid character
            if (error_pos) *error_pos = filename_error_pos;
            return CPAKFS_PARSE_ERR_FILENAME_CHAR;
        }
    }
    
    // Zero-fill remaining bytes in filename array
    memset(path->filename + n64_filename_len, 0, 16 - n64_filename_len);
    p = filename_end;

    // Parse extension (optional)
    if (*p == '.') {
        p++; // skip the dot
        const char *ext_start = p;
        int ext_utf8_len = strlen(ext_start);
        
        if (ext_utf8_len > 0) {
            // Convert extension from UTF-8 to N64 codepage
            const char *ext_error_pos;
            int n64_ext_len = utf8_to_n64(ext_start, ext_utf8_len, path->ext, 4, &ext_error_pos);
            if (n64_ext_len < 0) {
                if (n64_ext_len == -2) {
                    // Buffer overflow - extension too long
                    if (error_pos) *error_pos = ext_error_pos;
                    return CPAKFS_PARSE_ERR_EXTENSION_TOO_LONG;
                } else {
                    // Invalid character
                    if (error_pos) *error_pos = ext_error_pos;
                    return CPAKFS_PARSE_ERR_EXTENSION_CHAR;
                }
            }
            
            // Zero-fill remaining bytes in extension array
            memset(path->ext + n64_ext_len, 0, 4 - n64_ext_len);
        }
        // else: extension is empty, already zero-filled by memset above
    }

    return CPAKFS_PARSE_OK;
}

/**
 * @brief Append a single character to output buffer with bounds checking
 */
static inline bool append_char(char **out, char *end, char c)
{
    if (*out >= end) return false;
    **out = c;
    (*out)++;
    return true;
}

/**
 * @brief Append a hex digit to output buffer
 */
static inline bool append_hex(char **out, char *end, uint8_t value)
{
    char hex_chars[] = "0123456789ABCDEF";
    return append_char(out, end, hex_chars[value & 0xF]);
}

int cpakfs_path_format(const cpakfs_path_t *path, char *utf8_fullname, int buflen)
{
    if (!path || !utf8_fullname || buflen <= 1) {
        if (utf8_fullname && buflen > 0) utf8_fullname[0] = '\0';
        return -1;
    }

    char *out = utf8_fullname;
    char *end = utf8_fullname + buflen - 1; // Reserve space for null terminator

    // Format gamecode - try ASCII first, fallback to hex
    bool gamecode_is_ascii = true;
    for (int i = 0; i < 4; i++) {
        if (!is_valid_gamecode_char(path->gamecode[i])) {
            gamecode_is_ascii = false;
            break;
        }
    }
    
    if (gamecode_is_ascii) {
        for (int i = 0; i < 4; i++) {
            if (!append_char(&out, end, path->gamecode[i])) goto truncate;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            if (!append_hex(&out, end, path->gamecode[i] >> 4)) goto truncate;
            if (!append_hex(&out, end, path->gamecode[i])) goto truncate;
        }
    }

    // Append dot separator
    if (!append_char(&out, end, '.')) goto truncate;

    // Format pubcode - try ASCII first, fallback to hex
    bool pubcode_is_ascii = true;
    for (int i = 0; i < 2; i++) {
        if (!is_valid_gamecode_char(path->pubcode[i])) {
            pubcode_is_ascii = false;
            break;
        }
    }
    
    if (pubcode_is_ascii) {
        for (int i = 0; i < 2; i++) {
            if (!append_char(&out, end, path->pubcode[i])) goto truncate;
        }
    } else {
        for (int i = 0; i < 2; i++) {
            if (!append_hex(&out, end, path->pubcode[i] >> 4)) goto truncate;
            if (!append_hex(&out, end, path->pubcode[i])) goto truncate;
        }
    }

    // Append dash separator
    if (!append_char(&out, end, '-')) goto truncate;

    // Format filename directly
    int filename_len = n64_string_length(path->filename, 16);
    for (int i = 0; i < filename_len; i++) {
        char utf8_buf[4];
        int utf8_len = n64_to_utf8(path->filename[i], utf8_buf);
        for (int j = 0; j < utf8_len; j++) {
            if (!append_char(&out, end, utf8_buf[j])) goto truncate;
        }
    }

    // Format extension if present
    int ext_len = n64_string_length(path->ext, 4);
    if (ext_len > 0) {
        if (!append_char(&out, end, '.')) goto truncate;
        for (int i = 0; i < ext_len; i++) {
            char utf8_buf[4];
            int utf8_len = n64_to_utf8(path->ext[i], utf8_buf);
            for (int j = 0; j < utf8_len; j++) {
                if (!append_char(&out, end, utf8_buf[j])) goto truncate;
            }
        }
    }

    // Success - no truncation occurred
    *out = '\0';
    return 0;

truncate:
    // Buffer was too small - ensure null termination and return error
    *out = '\0';
    return -1;
}
