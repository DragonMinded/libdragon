/**
 * @file cpakfs_path.c
 * @author Giovanni Bajo <giovannibajo@gmail.com>
 * @brief Controller Pak Filesystem Routines
 * @ingroup controllerpak
 */
#include "cpakfs.h"
#include "cpakfs_internal.h"
#include <limits.h>
#ifdef N64
#include "debug.h"
#endif

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

/**
 * @brief Check whether a character is valid in the N64 codepage.
 * 
 * The N64 codepage is a 8-bit codepage where not all characters are valid.
 * This function checks whether the provided character is valid or not. Invalid
 * characters cannot be converted to UTF-8.
 */
bool __cpakfs_n64_string_valid(uint8_t ch)
{
    return ch == 0 || (ch >= 0x0F && ch <= 0x94);
}

/**
 * @brief Sanitize a string in the N64 codepage by replacing all invalid characters with NUL
 */
void __cpakfs_n64_string_sanitize(uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        if (!__cpakfs_n64_string_valid(buf[i])) {
            buf[i] = 0x00;
        }
    }
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

    assertf(0, "cpakfs: cannot convert invalid N64CP char %02X to UTF-8", c);
    return 0;
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

/**
 * @brief Find separators and split path into segments using simple pointer array
 */
static cpakfs_parse_err_t split_path_segments(const char *input, char *segments[5], const char **error_pos)
{
    const char *p = input;
    
    // segments[0] = start of gamecode
    segments[0] = (char*)p;
    
    // Find first dot (after gamecode)
    while (*p && *p != '.') p++;
    if (*p != '.') {
        if (error_pos) *error_pos = p;
        return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
    }
    segments[1] = (char*)p;  // segments[1] = end of gamecode (points to '.')
    p++;  // skip the dot
    
    // Find dash (after pubcode)
    while (*p && *p != '-') p++;
    if (*p != '-') {
        if (error_pos) *error_pos = p;
        return CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT;
    }
    segments[2] = (char*)p;  // segments[2] = end of pubcode (points to '-')
    p++;  // skip the dash
    
    // Find last dot in a single scan - use NULL as marker for "no dot found yet"
    segments[3] = NULL;  // Initially no extension found
    while (*p) {
        if (*p == '.') segments[3] = (char*)p;  // Update to point to the last dot found
        p++;
    }
    segments[4] = (char*)p;  // segments[4] = end of string (points to '\0')
    
    // If no dot was found, filename extends to end of string
    if (!segments[3]) segments[3] = segments[4];
    
    return CPAKFS_PARSE_OK;
}

/**
 * @brief Parse a code segment with unified logic using segment boundaries
 */
static cpakfs_parse_err_t parse_code_segment(char *start, char *end, uint8_t *output, 
                                           int normal_len, int hex_len,
                                           cpakfs_parse_err_t err_too_short,
                                           cpakfs_parse_err_t err_too_long,
                                           cpakfs_parse_err_t err_char,
                                           const char **error_pos)
{
    int length = end - start;
    
    if (length < normal_len) {
        if (error_pos) *error_pos = end;
        return err_too_short;
    }
    
    if (length == normal_len) {
        // Normal alphanumeric format
        for (int i = 0; i < normal_len; i++) {
            char c = start[i];
            if (!is_valid_gamecode_char(c)) {
                if (error_pos) *error_pos = start + i;
                return err_char;
            }
            // Convert to uppercase
            output[i] = (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
        }
        return CPAKFS_PARSE_OK;
    }
    
    if (length == hex_len) {
        // Check if all characters are hex
        for (int i = 0; i < hex_len; i++) {
            if (!is_hex_char(start[i])) {
                if (error_pos) *error_pos = start + normal_len;
                return err_too_long;
            }
        }
        // Parse as hex
        if (!parse_hex_string(start, hex_len, output, normal_len)) {
            if (error_pos) *error_pos = start;
            return err_char;
        }
        return CPAKFS_PARSE_OK;
    }
    
    // Wrong length
    if (error_pos) *error_pos = start + normal_len;
    return err_too_long;
}

cpakfs_parse_err_t cpakfs_path_parse(const char *utf8_fullname, cpakfs_path_t *path, const char **error_pos)
{
    if (!utf8_fullname || !path) {
        if (error_pos) *error_pos = utf8_fullname;
        return CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT;
    }

    // Clear the output structure
    memset(path, 0, sizeof(*path));

    // Split path into segments: 
    // [0]=gamecode_start, [1]=pubcode_start, [2]=filename_start, [3]=extension_start, [4]=end
    char *segments[5];
    cpakfs_parse_err_t result = split_path_segments(utf8_fullname, segments, error_pos);
    if (result != CPAKFS_PARSE_OK) {
        return result;
    }

    // Parse gamecode (segments[0] to segments[1])
    result = parse_code_segment(segments[0], segments[1], path->gamecode, 4, 8,
                               CPAKFS_PARSE_ERR_GAMECODE_TOO_SHORT,
                               CPAKFS_PARSE_ERR_GAMECODE_TOO_LONG,
                               CPAKFS_PARSE_ERR_GAMECODE_CHAR,
                               error_pos);
    if (result != CPAKFS_PARSE_OK) {
        return result;
    }

    // Parse pubcode (segments[1]+1 to segments[2], skip the '.')
    result = parse_code_segment(segments[1] + 1, segments[2], path->pubcode, 2, 4,
                               CPAKFS_PARSE_ERR_PUBCODE_TOO_SHORT,
                               CPAKFS_PARSE_ERR_PUBCODE_TOO_LONG,
                               CPAKFS_PARSE_ERR_PUBCODE_CHAR,
                               error_pos);
    if (result != CPAKFS_PARSE_OK) {
        return result;
    }

    // Parse filename (segments[2]+1 to segments[3], skip the '-')
    char *filename_start = segments[2] + 1;
    char *filename_end = segments[3];
    int filename_len = filename_end - filename_start;
    
    if (filename_len == 0) {
        if (error_pos) *error_pos = filename_start;
        return CPAKFS_PARSE_ERR_FILENAME_TOO_SHORT;
    }

    int n64_filename_len = utf8_to_n64(filename_start, filename_len, 
                                      path->filename, 16, error_pos);
    if (n64_filename_len < 0) {
        if (n64_filename_len == -2) {
            return CPAKFS_PARSE_ERR_FILENAME_TOO_LONG;
        } else {
            return CPAKFS_PARSE_ERR_FILENAME_CHAR;
        }
    }

    // Parse extension if present (segments[3]+1 to segments[4], skip the '.' if any)
    if (segments[3] < segments[4] && *segments[3] == '.') {
        char *ext_start = segments[3] + 1;
        char *ext_end = segments[4];
        int ext_len = ext_end - ext_start;
        
        if (ext_len > 0) {
            int n64_ext_len = utf8_to_n64(ext_start, ext_len,
                                         path->ext, 4, error_pos);
            if (n64_ext_len < 0) {
                if (n64_ext_len == -2) {
                    return CPAKFS_PARSE_ERR_EXTENSION_TOO_LONG;
                } else {
                    return CPAKFS_PARSE_ERR_EXTENSION_CHAR;
                }
            }
        }
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

/**
 * @brief Format a code (gamecode/pubcode) trying ASCII first, fallback to hex
 */
static bool append_code(char **out, char *end, const uint8_t *code, int len)
{
    // Try ASCII first
    bool is_ascii = true;
    for (int i = 0; i < len; i++) {
        if (!is_valid_gamecode_char(code[i])) {
            is_ascii = false;
            break;
        }
    }
    
    if (is_ascii) {
        for (int i = 0; i < len; i++) {
            if (!append_char(out, end, code[i])) return false;
        }
    } else {
        for (int i = 0; i < len; i++) {
            if (!append_hex(out, end, code[i] >> 4)) return false;
            if (!append_hex(out, end, code[i])) return false;
        }
    }
    return true;
}

/**
 * @brief Format a N64 string (filename/extension) to UTF-8
 */
static bool append_n64_string(char **out, char *end, const uint8_t *str, int max_len)
{
    int len = n64_string_length(str, max_len);
    for (int i = 0; i < len; i++) {
        char utf8_buf[4];
        int utf8_len = n64_to_utf8(str[i], utf8_buf);
        for (int j = 0; j < utf8_len; j++) {
            if (!append_char(out, end, utf8_buf[j])) return false;
        }
    }
    return true;
}

int cpakfs_path_format(const cpakfs_path_t *path, char *utf8_fullname, int buflen)
{
    if (!path || !utf8_fullname || buflen <= 1) {
        if (utf8_fullname && buflen > 0) utf8_fullname[0] = '\0';
        return -1;
    }

    char *out = utf8_fullname;
    char *end = utf8_fullname + buflen - 1; // Reserve space for null terminator

    if (!append_code(&out, end, path->gamecode, 4)) goto truncate;
    if (!append_char(&out, end, '.')) goto truncate;
    if (!append_code(&out, end, path->pubcode, 2)) goto truncate;
    if (!append_char(&out, end, '-')) goto truncate;
    if (!append_n64_string(&out, end, path->filename, 16)) goto truncate;
    int ext_len = n64_string_length(path->ext, 4);
    if (ext_len > 0) {
        if (!append_char(&out, end, '.')) goto truncate;
        if (!append_n64_string(&out, end, path->ext, 4)) goto truncate;
    }

    // Success - no truncation occurred
    *out = '\0';
    return 0;

truncate:
    // Buffer was too small - ensure null termination and return error
    *out = '\0';
    return -1;
}
