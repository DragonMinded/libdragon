/**
 * @file dir.c
 * @brief Directory handling
 * @ingroup system
 */
#include "dir.h"
#include "debug.h"
#include <stdbool.h>
#include <alloca.h>
#include <string.h>
#include <sys/errno.h>

int dir_walk(const char *path, dir_walk_callback_t cb, void *data)
{
    dir_t dir = {0};

    int fplen = strlen(path);
    char *fullpath = alloca(fplen+1+sizeof(dir.d_name)+1);
    strcpy(fullpath, path);
    if (fullpath[fplen-1] != '/')
        fullpath[fplen++] = '/';
    fullpath[fplen] = '\0';

    int ret = dir_findfirst(path, &dir);
    if (ret < 0) {
        if (ret == -1)
            return 0;
        return -1;  // errno already set
    }

    while (1) {
        strcpy(fullpath+fplen, dir.d_name);
        fullpath[fplen+strlen(dir.d_name)+1] = '\0';

        ret = cb(fullpath, &dir, data);
        if (ret < 0)
            return ret;
        if (ret == DIR_WALK_GOUP)
            return 0;
        if (dir.d_type == DT_DIR && ret != DIR_WALK_SKIPDIR) {
            ret = dir_walk(fullpath, cb, data);
            if (ret < 0)
                return ret;
        }

        ret = dir_findnext(path, &dir);
        if (ret < 0) {
            if (ret == -1)
                return 0;
            return -1;  // errno already set
        }
    }
}

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
static int pattern_matches_empty(const char *pattern) {
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
static fnmatch_result_t fnmatch_partial(const char *pattern, const char *path) {
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

bool dir_fnmatch(const char *pattern, const char *fullpath)
{
    return fnmatch_partial(pattern, fullpath) == FULL_MATCH;
}

int dir_glob(const char *pattern, const char *path, dir_walk_callback_t cb, void *data)
{
    int base_len = strlen(path);

    int callback(const char *fn, dir_t *dir, void *data)
    {
        fnmatch_result_t res = fnmatch_partial(pattern, fn+base_len);
        if (res == FULL_MATCH)
            return cb(fn, dir, data);
        if (dir->d_type == DT_DIR && res != PARTIAL_MATCH)
            return DIR_WALK_SKIPDIR;
        return DIR_WALK_CONTINUE;
    }

    return dir_walk(path, callback, data);
}
