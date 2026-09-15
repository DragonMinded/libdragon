#pragma once

#include <stdbool.h>

/** Return values for fnmatch_partial */
typedef enum {
    NO_MATCH = 0,
    PARTIAL_MATCH = 1,
    FULL_MATCH = 2
} fnmatch_result_t;

/*
 * Returns 1 if the remaining pattern can match an empty string.
 * For example, a pattern composed solely of '*' or '**' (possibly separated by '/')
 * can match an empty string, whereas '?' or any literal character cannot.
 */
static inline int pattern_matches_empty(const char *pattern) {
    while (*pattern != '\0') {
        if (*pattern == '*') {
            // If we find a "**" sequence followed by '/' or the end,
            // skip it.
            if (pattern[1] == '*' && (pattern[2] == '/' || pattern[2] == '\0')) {
                pattern += 2;
                if (*pattern == '/')
                    pattern++;
            } else {
                // A single '*' can match an empty sequence.
                pattern++;
            }
        } else {
            // A '?' or any literal character cannot match an empty string.
            return 0;
        }
    }
    return 1;
}

/*
 * fnmatch_partial()
 *
 * Compares the pattern with the path and returns:
 *   - FULL_MATCH if the path exactly matches the pattern.
 *   - PARTIAL_MATCH if the path is a valid prefix that could be
 *     extended in the future to obtain a complete match (useful during recursive directory search).
 *   - NO_MATCH if the path does not match the pattern and cannot be completed.
 *
 * The rules are as follows:
 *   - Literal characters must match exactly.
 *   - '?' matches any single character.
 *   - '*' matches a sequence of zero or more characters, but not the '/' separator.
 *   - '**' matches zero or more levels, including directory separators.
 */
static inline fnmatch_result_t fnmatch_partial(const char *pattern, const char *path) {
    // If we have reached the end of the pattern, the match is complete only if the path is also finished.
    if (*pattern == '\0')
        return (*path == '\0') ? FULL_MATCH : NO_MATCH;

    // If the path is finished, check if the rest of the pattern can match an empty string
    if (*path == '\0') {
        return pattern_matches_empty(pattern) ? FULL_MATCH : PARTIAL_MATCH;
    }

    // Handling the special "**" sequence
    if (pattern[0] == '*' && pattern[1] == '*' && (pattern[2] == '/' || pattern[2] == '\0')) {
        const char *next_pat = pattern + 2;
        if (*next_pat == '/')
            next_pat++;
        fnmatch_result_t res;
        const char *pp = path;
        // Try to match the remaining pattern with the current path and with every possible extension.
        do {
            res = fnmatch_partial(next_pat, pp);
            if (res == FULL_MATCH)
                return FULL_MATCH;
            if (res == PARTIAL_MATCH)
                return PARTIAL_MATCH;
            if (*pp == '\0')
                break;
            pp++;
        } while (1);
        return NO_MATCH;
    }

    // Handling the '*' character (which should not match '/')
    if (*pattern == '*') {
        const char *next_pat = pattern + 1;
        fnmatch_result_t res;
        const char *pp = path;
        do {
            res = fnmatch_partial(next_pat, pp);
            if (res == FULL_MATCH)
                return FULL_MATCH;
            if (res == PARTIAL_MATCH)
                return PARTIAL_MATCH;
            if (*pp == '\0' || *pp == '/')
                break;
            pp++;
        } while (1);
        return NO_MATCH;
    }

    // Handling the '?' character (matches exactly one character)
    if (*pattern == '?')
        return fnmatch_partial(pattern + 1, path + 1);

    // Handling literal matching: the characters must be identical.
    if (*pattern == *path)
        return fnmatch_partial(pattern + 1, path + 1);

    // If none of the cases match, there is no match.
    return NO_MATCH;
}

/**
 * Simple fnmatch implementation for pattern matching
 */
static inline bool fnmatch(const char *pattern, const char *path) {
    return fnmatch_partial(pattern, path) == FULL_MATCH;
}
