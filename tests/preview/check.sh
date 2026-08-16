#!/usr/bin/env bash

# Check that preview APIs are locked for a stable project and unlocked for a
# project that opted in with LIBDRAGON_PREVIEW=1.
#
# This is what makes the single-branch model safe to rely on: a stable project
# must keep building even though the headers it includes also declare preview
# APIs, and it must not be able to reach those APIs by accident.

set -uo pipefail

: "${N64_CC:?}"
: "${N64_CFLAGS:?}"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

failures=0

# build <stable|preview> <source>
# Compiles a snippet, leaving the diagnostics in $output and the compiler exit
# code in $status. This has to be a real compilation rather than -fsyntax-only,
# as the error attribute is only reported once the compiler knows that the call
# survived optimization.
build() {
    local preview=""
    [ "$1" = preview ] && preview=-DLIBDRAGON_PREVIEW
    printf '%s\n' "$2" > "$tmp/snippet.c"
    output=$($N64_CC $N64_CFLAGS $preview -fdiagnostics-color=never \
        -c -o "$tmp/snippet.o" "$tmp/snippet.c" 2>&1)
    status=$?
}

fail() {
    echo "FAIL: $*"
    failures=$((failures + 1))
}

# expect <stable|preview> <compiles|rejected> <description> <source>
expect() {
    build "$1" "$4"
    case "$2" in
    compiles)
        if [ $status -ne 0 ]; then
            fail "[$1] $3: expected to compile"
            echo "$output"
        fi
        ;;
    rejected)
        if [ $status -eq 0 ]; then
            fail "[$1] $3: expected to be rejected, but it compiled"
        fi
        ;;
    esac
}

# expect_diagnostic <stable|preview> <description> <source>
# Like expect, but only requires the preview message to be reported, without
# caring whether it was reported as an error or as a warning.
expect_diagnostic() {
    build "$1" "$3"
    if ! grep -q 'libdragon preview API' <<< "$output"; then
        fail "[$1] $2: expected a preview diagnostic"
        echo "$output"
    fi
}

# A stable project must keep building even though libdragon.h also declares
# preview APIs.
expect stable compiles "including libdragon.h" '
#include <libdragon.h>
'
expect preview compiles "including libdragon.h" '
#include <libdragon.h>
'

# Headers that the user includes explicitly are locked as a whole.
for header in GL/gl.h GL/glu.h GL/gl_integration.h; do
    expect stable rejected "including $header" "
#include <$header>
"
    expect preview compiles "including $header" "
#include <$header>
"
done

# Single entry points reached through libdragon.h are locked one at a time.
expect stable rejected "calling a preview function" '
#include <libdragon.h>
float f(void) { return model64_anim_get_length(0, "anim"); }
'
expect preview compiles "using a preview API" '
#include <libdragon.h>
float f(void) { model64_t *m = model64_load("x"); return model64_anim_get_length(m, "anim"); }
'

# Taking the address of a preview function sidesteps the error attribute, so
# only the deprecated backstop reports it.
expect_diagnostic stable "taking the address of a preview function" '
#include <libdragon.h>
void *p = (void*)model64_free;
'

if [ $failures -ne 0 ]; then
    echo "$failures preview check(s) failed"
    exit 1
fi
