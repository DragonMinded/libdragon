/*
    n64sym: generate a symbol table for an N64 ROM
    Written by Giovanni Bajo <giovannibajo@gmail.com>

    This tool is part of the Libdragon SDK.

    This is free and unencumbered software released into the public domain.
    For more information, please refer to <http://unlicense.org/>
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool is_space_char(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool no_space_before(char c)
{
    switch (c) {
    case ',': case ')': case '>': case '&':
    case '*': case ']': case ':': return true;
    default: return false;
    }
}

static bool no_space_after(char c)
{
    switch (c) {
    case '(': case '<':
    case '[': case ':': return true;
    default: return false;
    }
}

/* Normalize spacing in demangled signatures:
 * collapse runs of whitespace and keep at most one space where needed.
 * Spaces around punctuation like ',', '(', ')', '<', '>', ':', '&', '*'
 * are removed when syntactically safe.
 */
static char *normalize_whitespace(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    size_t o = 0;
    bool pending_space = false;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (is_space_char(c)) {
            pending_space = true;
            continue;
        }
        if (pending_space) {
            if (o > 0 && !no_space_before(c) && !no_space_after(out[o-1]))
                out[o++] = ' ';
            pending_space = false;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return out;
}

static char *replace_all(const char *s, const char *from, const char *to)
{
    size_t sl = strlen(s), fl = strlen(from), tl = strlen(to);
    assert(fl > 0);

    size_t count = 0;
    for (const char *p = s; (p = strstr(p, from)); p += fl) count++;
    if (!count)
        return strdup(s);

    size_t out_len = sl + count * (tl - fl);
    char *out = malloc(out_len + 1);
    char *w = out;
    const char *p = s;
    const char *m;
    while ((m = strstr(p, from))) {
        size_t n = (size_t)(m - p);
        memcpy(w, p, n);
        w += n;
        memcpy(w, to, tl);
        w += tl;
        p = m + fl;
    }
    strcpy(w, p);
    return out;
}

/* Replace known verbose canonical type spellings with shorter aliases.
 * This includes std::basic_string variants to std::string and
 * C integer spellings like "unsigned char" -> "u8".
 * Pure textual replacement, deterministic and order-preserving.
 */
static char *compress_std_types(const char *s)
{
    static const struct { const char *from, *to; } repls[] = {
        { "(anonymous namespace)", "(anon)" },
        { "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>::basic_string", "std::string::string" },
        { "std::basic_string<char, std::char_traits<char>, std::allocator<char>>::basic_string", "std::string::string" },
        { "std::__cxx11::basic_string<char,std::char_traits<char>,std::allocator<char>>", "std::string" },
        { "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string" },
        { "std::basic_string<char,std::char_traits<char>,std::allocator<char>>", "std::string" },
        { "std::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string" },
        { "unsigned long long", "u64" },
        { "unsigned long", "u64" },
        { "unsigned int", "u32" },
        { "unsigned short", "u16" },
        { "unsigned char", "u8" },
        { "signed char", "i8" },
        { "long long", "i64" },
        { "signed int", "i32" },
    };

    char *cur = strdup(s);
    for (size_t i = 0; i < sizeof(repls)/sizeof(repls[0]); i++) {
        char *next = replace_all(cur, repls[i].from, repls[i].to);
        free(cur);
        cur = next;
    }
    return cur;
}

/* Replace template argument lists with "<...>".
 * The parser tracks nested '<' '>' depth so nested templates collapse
 * to a single placeholder per top-level template occurrence.
 * This preserves surrounding symbol structure while shrinking size.
 */
static char *collapse_template_args(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    size_t o = 0;

    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c != '<' || (i + 1 < len && s[i+1] == '<')) {
            out[o++] = c;
            continue;
        }

        int depth = 1;
        size_t j = i + 1;
        for (; j < len; j++) {
            if (s[j] == '<') depth++;
            else if (s[j] == '>') {
                depth--;
                if (depth == 0) break;
            }
        }
        if (j >= len || s[j] != '>') {
            out[o++] = c;
            continue;
        }

        size_t span_len = j - i + 1; // includes both '<' and '>'
        if (span_len >= 5) {
            out[o++] = '<';
            out[o++] = '.';
            out[o++] = '.';
            out[o++] = '.';
            out[o++] = '>';
        } else {
            // Keep the original span if replacing would grow output.
            memcpy(out + o, s + i, span_len);
            o += span_len;
        }
        i = j;
    }

    assert(o <= len);
    out[o] = '\0';
    return out;
}

/* Abbreviate one namespace component to initials.
 * If it starts with '_' or '__', preserve leading underscores and append
 * one significant character (eg: "__cxxabi" -> "__c"). */
static size_t append_short_component(const char *s, size_t len, char *out, size_t o)
{
    size_t i = 0;
    while (i < len && s[i] == '_')
        out[o++] = s[i++];

    if (i < len) {
        for (size_t j = i; j < len; j++) {
            if (isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == '~') {
                out[o++] = s[j];
                return o;
            }
        }
        out[o++] = s[i];
        return o;
    }

    if (len > 0)
        out[o++] = '_';
    return o;
}

static bool is_namespace_span_break(char c)
{
    switch (c) {
    case ' ': case '\t': case '\r': case '\n':
    case ',': case '(': case ')': case '&':
    case '*': case '[': case ']': return true;
    default: return false;
    }
}

static size_t compress_namespace_span(const char *s, size_t len, char *out)
{
    size_t starts[64], ends[64], ncomp = 0;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '<') depth++;
        else if (s[i] == '>' && depth > 0) depth--;
        else if (depth == 0 && i + 1 < len && s[i] == ':' && s[i+1] == ':') {
            if (ncomp < 64) {
                starts[ncomp] = start;
                ends[ncomp] = i;
                ncomp++;
            }
            start = i + 2;
            i++;
        }
    }
    if (ncomp < 64) {
        starts[ncomp] = start;
        ends[ncomp] = len;
        ncomp++;
    }
    if (ncomp <= 2) {
        memcpy(out, s, len);
        return len;
    }

    size_t o = 0;
    for (size_t i = 0; i < ncomp; i++) {
        size_t seg_len = ends[i] - starts[i];
        if (i < ncomp - 2) {
            if (seg_len == 3 && strncmp(s + starts[i], "std", 3) == 0) {
                memcpy(out + o, s + starts[i], seg_len);
                o += seg_len;
            } else {
                o = append_short_component(s + starts[i], seg_len, out, o);
            }
        } else {
            memcpy(out + o, s + starts[i], seg_len);
            o += seg_len;
        }
        if (i + 1 < ncomp) {
            out[o++] = ':';
            out[o++] = ':';
        }
    }
    return o;
}

/* Compress namespace/class scope in the callable prefix.
 * For chains A::B::C::Func, intermediate components are reduced to
 * initials while preserving the last two components in full.
 * Scope parsing ignores '::' inside template argument depth.
 */
static char *compress_namespaces(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    size_t o = 0, span_start = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || is_namespace_span_break(s[i])) {
            size_t span_len = i - span_start;
            if (span_len)
                o += compress_namespace_span(s + span_start, span_len, out + o);
            if (i < len)
                out[o++] = s[i];
            span_start = i + 1;
        }
    }
    out[o] = '\0';
    return out;
}

/* In parameter types, drop the exact owning scope of the function.
 * Example: X::Y::Z::f(..., X::Y::Z::Data*) -> ...f(..., Data*).
 * Function name prefix is left untouched; only argument substring
 * after '(' is rewritten.
 */
static char *elide_own_scope_in_args(const char *s)
{
    const char *paren = strchr(s, '(');
    if (!paren)
        return strdup(s);

    size_t prefix_len = (size_t)(paren - s);
    const char *last_scope = NULL;
    int depth = 0;
    for (size_t i = 0; i + 1 < prefix_len; i++) {
        if (s[i] == '<') depth++;
        else if (s[i] == '>' && depth > 0) depth--;
        else if (depth == 0 && s[i] == ':' && s[i+1] == ':')
            last_scope = s + i;
    }
    if (!last_scope)
        return strdup(s);

    size_t scope_len = (size_t)((last_scope + 2) - s); /* include trailing :: */
    if (scope_len == 0)
        return strdup(s);

    size_t len = strlen(s);
    char *out = malloc(len + 1);
    size_t o = 0;

    memcpy(out, s, prefix_len);
    o += prefix_len;

    for (size_t i = prefix_len; i < len; ) {
        if (i + scope_len <= len && strncmp(s + i, s, scope_len) == 0) {
            i += scope_len;
            continue;
        }
        out[o++] = s[i++];
    }
    out[o] = '\0';
    return out;
}

/* Final width guard: hard truncate to max_len characters.
 * If truncation is required and max_len > 3, reserve the suffix "...".
 * This is the last-resort step after semantic shortening passes.
 */
static char *fallback_truncate(const char *s, int max_len)
{
    assert(max_len > 0);
    size_t len = strlen(s);
    if ((int)len <= max_len)
        return strdup(s);

    char *out = malloc((size_t)max_len + 1);
    if (max_len <= 3) {
        memcpy(out, s, (size_t)max_len);
        out[max_len] = '\0';
        return out;
    }
    memcpy(out, s, (size_t)max_len);
    memcpy(out + max_len - 3, "...", 3);
    out[max_len] = '\0';
    return out;
}

/* Shorten one demangled C++ symbol to improve readability while respecting
 * max_len. The pipeline applies deterministic passes (whitespace/type/scope
 * reductions), then progressively more aggressive compaction, and finally a
 * hard truncation fallback with "...".
 * Returns a newly allocated string owned by the caller. */
char *cpp_shorten_symbol(const char *sym, int max_len)
{
    assert(sym != NULL);
    assert(max_len > 0);
    char *cur = strdup(sym);
    char *next = normalize_whitespace(cur);
    free(cur); cur = next;

    next = compress_std_types(cur);
    free(cur); cur = next;

    next = elide_own_scope_in_args(cur);
    free(cur); cur = next;
    if ((int)strlen(cur) <= max_len)
        return cur;

    next = collapse_template_args(cur);
    free(cur); cur = next;
    if ((int)strlen(cur) <= max_len)
        return cur;

    next = compress_namespaces(cur);
    free(cur); cur = next;
    if ((int)strlen(cur) <= max_len)
        return cur;
    
    next = normalize_whitespace(cur);
    free(cur); cur = next;
    if ((int)strlen(cur) <= max_len)
        return cur;

    next = fallback_truncate(cur, max_len);
    free(cur);
    return next;
}
