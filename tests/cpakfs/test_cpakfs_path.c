/**
 * @file test_cpakfs_path.c
 * @brief Test for cpakfs path parsing and formatting functions
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <errno.h>

#define assertf(condition, fmt, ...) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed: " fmt "\n", ##__VA_ARGS__); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define __randn(n) (rand() % (n))

// Include the cpakfs header first for types
#include "../../include/cpakfs.h"

// Include the actual cpakfs_path.c file
#include "../../src/joybus/cpakfs_path.c"

// Test framework
static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: " #name "... "); \
        tests_run++; \
        test_##name(); \
        printf("PASSED\n"); \
    } \
    static void test_##name(void)

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAILED\n  Expected: %d, Got: %d at line %d\n", (int)(expected), (int)(actual), __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAILED\n  Expected: \"%s\", Got: \"%s\" at line %d\n", (expected), (actual), __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_MEM_EQ(expected, actual, len) \
    do { \
        if (memcmp((expected), (actual), (len)) != 0) { \
            printf("FAILED\n  Memory mismatch at line %d\n", __LINE__); \
            tests_failed++; \
            return; \
        } \
    } while(0)

// Data-driven test structure for valid paths
typedef struct {
    const char *description;
    const char *utf8_fullname;
    cpakfs_path_t expected_path;
} path_test_case_t;

static const path_test_case_t valid_path_tests[] = {
    {
        .description = "Basic ASCII path with extension",
        .utf8_fullname = "NSME.01-SAVE.A",
        .expected_path = { "NSME", "01", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Lowercase conversion",
        .utf8_fullname = "nsme.01-save.a",
        .expected_path = { "NSME", "01", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Path without extension",
        .utf8_fullname = "GAME.01-FILENAME",
        .expected_path = { "GAME", "01", "\x1F\x22\x25\x1E\x27\x1A\x26\x1E", "" },
    },
    {
        .description = "Numeric gamecode and pubcode",
        .utf8_fullname = "1234.56-SAVE.A",
        .expected_path = { "1234", "56", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Mixed alphanumeric codes",
        .utf8_fullname = "TEST.99-MYFILE.DAT",
        .expected_path = { "TEST", "99", "\x26\x32\x1F\x22\x25\x1E", "\x1D\x1A\x2D" },
    },
    {
        .description = "Embedded NUL characters in filename",
        .utf8_fullname = "GAME.01-FOO\001BAR.TXT",
        .expected_path = { "GAME", "01", "\x1F\x28\x28\x00\x1B\x1A\x2B", "\x2D\x31\x2D" },
    },
    {
        .description = "Hex gamecode with normal pubcode",
        .utf8_fullname = "04050607.01-SAVE.A",
        .expected_path = { "\x04\x05\x06\x07", "01", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Normal gamecode with hex pubcode", 
        .utf8_fullname = "GAME.FF00-SAVE.A",
        .expected_path = { "GAME", "\xFF\x00", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Both gamecode and pubcode in hex",
        .utf8_fullname = "01234567.ABCD-SAVE.A",
        .expected_path = { "\x01\x23\x45\x67", "\xAB\xCD", "\x2C\x1A\x2F\x1E", "\x1A" },
    },
    {
        .description = "Hex with lowercase letters",
        .utf8_fullname = "deadbeef.cafe-SAVE.A",
        .expected_path = { "\xDE\xAD\xBE\xEF", "\xCA\xFE", "\x2C\x1A\x2F\x1E", "\x1A" },
    }
};

// Data-driven test structure for invalid paths
typedef struct {
    const char *description;
    const char *utf8_fullname;
    cpakfs_parse_err_t expected_error;
} path_error_test_case_t;

static const path_error_test_case_t error_path_tests[] = {
    {
        .description = "Too short gamecode",
        .utf8_fullname = "GAM.01-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_GAMECODE_LEN,
    },
    {
        .description = "Too short pubcode",
        .utf8_fullname = "GAME.0-SAVE.A", 
        .expected_error = CPAKFS_PARSE_ERR_PUBCODE_LEN,
    },
    {
        .description = "Invalid character in gamecode",
        .utf8_fullname = "GA@E.01-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_GAMECODE_CHAR,
    },
    {
        .description = "Empty filename",
        .utf8_fullname = "GAME.01-.A",
        .expected_error = CPAKFS_PARSE_ERR_FILENAME_LEN,
    },
    {
        .description = "Invalid hex gamecode (non-hex chars)",
        .utf8_fullname = "GAMECODE.01-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_GAMECODE_LEN,
    },
    {
        .description = "Invalid hex pubcode (non-hex chars)",
        .utf8_fullname = "GAME.PUBZ-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_PUBCODE_LEN,
    },
    {
        .description = "Wrong gamecode length (6 chars)",
        .utf8_fullname = "ABCDEF.01-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_GAMECODE_LEN,
    },
    {
        .description = "Wrong pubcode length (3 chars)",
        .utf8_fullname = "GAME.ABC-SAVE.A",
        .expected_error = CPAKFS_PARSE_ERR_PUBCODE_LEN,
    }
};

// Test cases
TEST(valid_paths_roundtrip) {
    const int num_tests = sizeof(valid_path_tests) / sizeof(valid_path_tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        const path_test_case_t *test = &valid_path_tests[i];
        cpakfs_path_t path;
        const char *error_pos;
        char formatted[64];
        
        printf("  Sub-test %d: %s\n", i + 1, test->description);
        
        // Test parsing
        cpakfs_parse_err_t result = cpakfs_path_parse(test->utf8_fullname, &path, &error_pos);
        if (result != CPAKFS_PARSE_OK) {
            printf("FAILED\n  Parse error: %d for '%s'\n", result, test->utf8_fullname);
            tests_failed++;
            return;
        }
        
        // Check parsed values - just compare the entire structure
        if (memcmp(&path, &test->expected_path, sizeof(cpakfs_path_t)) != 0) {
            printf("FAILED\n  Path structure mismatch for '%s'\n", test->utf8_fullname);
            printf("    Expected gamecode: %.4s\n", test->expected_path.gamecode);
            printf("    Got gamecode:      %.4s\n", path.gamecode);
            printf("    Expected pubcode:  %.2s\n", test->expected_path.pubcode);
            printf("    Got pubcode:       %.2s\n", path.pubcode);
            tests_failed++;
            return;
        }
        
        // Test formatting
        cpakfs_path_format(&path, formatted, sizeof(formatted));
        
        // For inputs with lowercase, the formatted result should be uppercase
        const char *expected_formatted = test->utf8_fullname;
        if (strcmp(test->utf8_fullname, "nsme.01-save.a") == 0) {
            expected_formatted = "NSME.01-SAVE.A";
        } else if (strcmp(test->utf8_fullname, "deadbeef.cafe-SAVE.A") == 0) {
            expected_formatted = "DEADBEEF.CAFE-SAVE.A";
        }
        
        if (strcmp(formatted, expected_formatted) != 0) {
            printf("FAILED\n  Format mismatch for '%s': expected '%s', got '%s'\n", 
                   test->utf8_fullname, expected_formatted, formatted);
            tests_failed++;
            return;
        }
        
        // Test roundtrip consistency
        cpakfs_path_t path2;
        cpakfs_parse_err_t result2 = cpakfs_path_parse(formatted, &path2, &error_pos);
        if (result2 != CPAKFS_PARSE_OK) {
            printf("FAILED\n  Roundtrip parse error: %d for '%s'\n", result2, formatted);
            tests_failed++;
            return;
        }
        
        if (memcmp(&path, &path2, sizeof(cpakfs_path_t)) != 0) {
            printf("FAILED\n  Roundtrip data mismatch for '%s'\n", test->utf8_fullname);
            tests_failed++;
            return;
        }
    }
}

TEST(invalid_paths) {
    const int num_tests = sizeof(error_path_tests) / sizeof(error_path_tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        const path_error_test_case_t *test = &error_path_tests[i];
        cpakfs_path_t path;
        const char *error_pos;
        
        printf("  Sub-test %d: %s\n", i + 1, test->description);
        
        cpakfs_parse_err_t result = cpakfs_path_parse(test->utf8_fullname, &path, &error_pos);
        if (result != test->expected_error) {
            printf("FAILED\n  Expected error %d, got %d for '%s'\n", 
                   test->expected_error, result, test->utf8_fullname);
            tests_failed++;
            return;
        }
    }
}

TEST(null_inputs) {
    cpakfs_path_t path;
    const char *error_pos;
    
    // Test null input
    cpakfs_parse_err_t result = cpakfs_path_parse(NULL, &path, &error_pos);
    ASSERT_EQ(CPAKFS_PARSE_ERR_GAMECODE_LEN, result);
    
    // Test null path
    result = cpakfs_path_parse("GAME.01-SAVE.A", NULL, &error_pos);
    ASSERT_EQ(CPAKFS_PARSE_ERR_GAMECODE_LEN, result);
    
    // Test null formatting inputs
    char output[64];
    cpakfs_path_format(NULL, output, sizeof(output));
    ASSERT_STR_EQ("", output);
}

int main() {
    printf("Running cpakfs path tests...\n\n");
    
    run_test_valid_paths_roundtrip();
    run_test_invalid_paths();
    run_test_null_inputs();
    
    printf("\nTests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed > 0) {
        printf("SOME TESTS FAILED!\n");
        return 1;
    } else {
        printf("ALL TESTS PASSED!\n");
        return 0;
    }
}
