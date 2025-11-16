#! /bin/bash
# N64 MIPS GCC toolchain build/install script for Unix distributions
# (c) 2012-2025 DragonMinded and LibDragon Contributors.
# Licensed under the Unlicense. See LICENSE.md for details.
#
# This script builds a toolchain for the N64. It is a standard GCC cross-compiler
# with target "mips64-elf".
#
# We build two sysroots:
# - One with newlib as libc, in $N64_INST/$N64_TARGET/newlib
# - One with picolibc as libc, in $N64_INST/$N64_TARGET/picolibc
#
# For backward compatibility, we also build symlinks so that
# $N64_INST/$N64_TARGET/{include,lib} point to the newlib sysroot, which makes
# sure that old build scripts will still work with this toolchain. Modern
# libdragon build scripts instead know about these two sysroots and select the
# correct directory depending on the configuration.
#

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
set -x
IFS=$'\n\t'

# Check that N64_INST is defined
if [ -z "${N64_INST-}" ]; then
    echo "N64_INST environment variable is not defined."
    echo "Please define N64_INST and point it to the requested installation directory"
    exit 1
fi

# Path where the toolchain will be built.
BUILD_PATH="${BUILD_PATH:-toolchain}"
DOWNLOAD_PATH="${DOWNLOAD_PATH:-$BUILD_PATH}"

# Redirect output to a log file
exec > >(tee "$BUILD_PATH/build-toolchain.log") 2>&1
echo "Build started at: $(date)"

# Additional directories
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_PATH="$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")"

# Defines the build system variables to allow cross compilation.
N64_BUILD=${N64_BUILD:-""}
N64_HOST=${N64_HOST:-""}
N64_TARGET=${N64_TARGET:-mips64-elf}

# Set N64_INST before calling the script to change the default installation directory path
INSTALL_PATH="${N64_INST}"
# Set PATH for newlib to compile using GCC for MIPS N64 (pass 1)
export PATH="$PATH:$INSTALL_PATH/bin"

# Determine how many parallel Make jobs to run based on CPU count
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

# GCC configure arguments to use system GMP/MPC/MFPF
BINUTILS_CONFIGURE_ARGS=()
GCC_CONFIGURE_ARGS=()

# Dependency source libs (Versions)
BINUTILS_V=2.44
GCC_V=14.2.0
NEWLIB_V=4.4.0.20231231
PICOLIBC_V=34af875350c3319f1c83b5490a8edf2c8379f231
GMP_V=6.3.0
MPC_V=1.3.1
MPFR_V=4.2.1
ZLIB_V=${ZLIB_V:-""}
MAKE_V=${MAKE_V:-""}

# Create build and download directories
mkdir -p "$BUILD_PATH" "$DOWNLOAD_PATH"

# Resolve absolute paths for build and download directories
BUILD_PATH=$(cd "$BUILD_PATH" && pwd)
DOWNLOAD_PATH=$(cd "$DOWNLOAD_PATH" && pwd)

# Check if a command-line tool is available: status 0 means "yes"; status 1 means "no"
command_exists () {
    (command -v "$1" >/dev/null 2>&1)
    return $?
}

# Automatically run the command with sudo/su if needed.
autosudo() {
    "$@" && return 0

    if command_exists sudo; then
        sudo env PATH="$PATH" "$@" && return 0
    fi

    if command_exists su; then
        su -c "env PATH=\"$PATH\" $*"
        return $?
    fi

    return 1
}

# Download the file URL using wget or curl (depending on which is installed)
download () {
    local url="$1"
    local file
    file="$DOWNLOAD_PATH/$(basename "$url")"
    local tmpfile="$file.part"
    if   command_exists wget ; then wget --continue --output-document "$tmpfile" "$url"
    elif command_exists curl ; then curl --location --output "$tmpfile" "$url"
    else
        echo "Install wget or curl to download toolchain sources" 1>&2
        return 1
    fi
    mv "$tmpfile" "$file"
}

# Install ktls header into the toolchain sysroot.
# The content of this file is at the end of this script.
install_ktls_header () {
    local prefix="$1"

    local tmpfile
    tmpfile=$(mktemp)
    awk -v start=": <<'__KTLS_H_BLOCK__'" -v end="__KTLS_H_BLOCK__" '
        $0 == start {capture=1; next}
        $0 == end {exit}
        capture {print}
    ' "$SCRIPT_PATH" > "$tmpfile"

    local dest_dir="$prefix/$N64_TARGET/include"
    autosudo mkdir -p "$dest_dir"
    autosudo install -m 0644 "$tmpfile" "$dest_dir/ktls.h"

    rm -f "$tmpfile"
}

# Patch the GCC installation to force include of ktls.h
patch_gcc_specs () {
    local phase="${1:-0}"
    local src_gcc_bin="$CROSS_PREFIX/bin/$N64_TARGET-gcc"

    if [ ! -x "$src_gcc_bin" ]; then
        if [ -x "$src_gcc_bin.exe" ]; then
            src_gcc_bin="$src_gcc_bin.exe"
        else
            echo "GCC binary not found: $src_gcc_bin" >&2
            exit 1
        fi
    fi

    local specs_tmp
    local patched_tmp
    local specs_dest_dir=""
    local specs_dest=""
    local marker=""
    local dest_prefix=""
    specs_tmp=$(mktemp)
    patched_tmp=$(mktemp)
    trap 'trap - RETURN EXIT; rm -f "$specs_tmp" "$patched_tmp"' RETURN EXIT

    if ! "$src_gcc_bin" -dumpspecs > "$specs_tmp"; then
        echo "Failed to dump GCC specs from $src_gcc_bin" >&2
        exit 1
    fi

    case "$phase" in
        0)
            # Phase 0: install specs with absolute include path into the BUILD compiler.
            dest_prefix="$CROSS_PREFIX"
            marker="-include $dest_prefix/$N64_TARGET/include/ktls.h"
            ;;
        1)
            # Phase 1: install specs with relocatable include path into the distributable compiler.
            dest_prefix="$INSTALL_PATH"
            marker="-include ktls.h"
            ;;
        *)
            echo "Unknown specs patch phase: $phase" >&2
            exit 1
            ;;
    esac
    local prefixed_marker="$marker "
    local escaped_prefixed_marker="${prefixed_marker//&/\&}"
    escaped_prefixed_marker="${escaped_prefixed_marker//|/\|}"
    local deletion_regex='-include[[:space:]]*[^[:space:]]*ktls\.h'

    # Patch spec file, adding a forced include in both cpp_options and cc1_options.
    # cpp_options is used when running the preprocessor only, and cc1_options
    # is used when compiling. In both cases, we want ktls.h to be included.
    sed \
        -e "/^\*cpp_options:$/,/^\*/{ /$deletion_regex/d; }" \
        -e "/^\*cc1_options:$/,/^\*/{ /$deletion_regex/d; }" \
        -e "/^\*cpp_options:$/{" \
        -e "n" \
            -e "s|^|$escaped_prefixed_marker|" \
        -e "}" \
        -e "/^\*cc1_options:$/{" \
        -e "n" \
            -e "s|^|$escaped_prefixed_marker|" \
        -e "}" \
        "$specs_tmp" > "$patched_tmp"

    # Just verify the patch was successful (sed does not error out if no
    # match is found).
    if ! grep -qF -- "$marker" "$patched_tmp"; then
        echo "INTERNAL ERROR: failed to patch GCC specs" >&2
        exit 1
    fi

    # Determine destination directory; prefer existing lib/ path, fall back to lib64/ if needed.
    local candidate
    for candidate in \
        "$dest_prefix/lib/gcc/$N64_TARGET/$GCC_V" \
        "$dest_prefix/lib64/gcc/$N64_TARGET/$GCC_V"; do
        if [ -z "$specs_dest_dir" ] && [ -d "$candidate" ]; then
            specs_dest_dir="$candidate"
        fi
    done
    if [ -z "$specs_dest_dir" ]; then
        specs_dest_dir="$dest_prefix/lib/gcc/$N64_TARGET/$GCC_V"
    fi
    specs_dest="$specs_dest_dir/specs"

    # Install patched spec file
    autosudo mkdir -p "$specs_dest_dir"
    autosudo install -m 0644 "$patched_tmp" "$specs_dest"
}

# Compilation on macOS via homebrew
if [[ $OSTYPE == 'darwin'* ]]; then
    if ! command_exists brew; then
        echo "Compilation on macOS is supported via Homebrew (https://brew.sh)"
        echo "Please install homebrew and try again"
        exit 1
    fi

    # Install required dependencies. gsed is really required, the others are optionals
    # and just speed up build.
    # zlib is part of the base OS, and doesn't need to be installed here.
    brew install -q gmp mpfr libmpc gsed isl make python3 texinfo ninja meson

    # FIXME: we could avoid download/symlink GMP and friends for a cross-compiler
    # but we need to symlink them for the canadian compiler.
    #GMP_V=""
    #MPC_V=""
    #MPFR_V=""

    # Tell Binutils and GCC configure where to find the dependent libraries
    BINUTILS_CONFIGURE_ARGS=(
        "--with-gmp=$(brew --prefix gmp)"
        "--with-mpfr=$(brew --prefix mpfr)"
        "--with-mpc=$(brew --prefix libmpc)"
        "--with-isl=$(brew --prefix isl)"
        "--with-system-zlib"
    )
    GCC_CONFIGURE_ARGS=(
        "--with-gmp=$(brew --prefix gmp)"
        "--with-mpfr=$(brew --prefix mpfr)"
        "--with-mpc=$(brew --prefix libmpc)"
        "--with-isl=$(brew --prefix isl)"
        "--with-system-zlib"
    )

    # Install GNU sed as default sed in PATH. GCC compilation fails otherwise,
    # because it does not work with BSD sed.
    PATH="$(brew --prefix gsed)/libexec/gnubin:$PATH"
    export PATH
else
    # Configure Binutils and GCC arguments for non-macOS platforms
    BINUTILS_CONFIGURE_ARGS+=("--with-system-zlib")
    GCC_CONFIGURE_ARGS+=("--with-system-zlib")
fi

# Dependency downloads and unpack
test -f "$DOWNLOAD_PATH/binutils-$BINUTILS_V.tar.gz" || download "https://ftpmirror.gnu.org/gnu/binutils/binutils-$BINUTILS_V.tar.gz"
test -d "$BUILD_PATH/binutils-$BINUTILS_V"           || tar -xzf "$DOWNLOAD_PATH/binutils-$BINUTILS_V.tar.gz" -C "$BUILD_PATH"

test -f "$DOWNLOAD_PATH/gcc-$GCC_V.tar.gz"           || download "https://ftpmirror.gnu.org/gnu/gcc/gcc-$GCC_V/gcc-$GCC_V.tar.gz"
test -d "$BUILD_PATH/gcc-$GCC_V"                     || tar -xzf "$DOWNLOAD_PATH/gcc-$GCC_V.tar.gz" -C "$BUILD_PATH"

test -f "$DOWNLOAD_PATH/newlib-$NEWLIB_V.tar.gz"     || download "https://sourceware.org/pub/newlib/newlib-$NEWLIB_V.tar.gz"
test -d "$BUILD_PATH/newlib-$NEWLIB_V"               || tar -xzf "$DOWNLOAD_PATH/newlib-$NEWLIB_V.tar.gz" -C "$BUILD_PATH"

test -f "$DOWNLOAD_PATH/picolibc-$PICOLIBC_V.zip"    || ( download "https://github.com/picolibc/picolibc/archive/$PICOLIBC_V.zip" && mv "$DOWNLOAD_PATH/$PICOLIBC_V.zip" "$DOWNLOAD_PATH/picolibc-$PICOLIBC_V.zip" )
test -d "$BUILD_PATH/picolibc-$PICOLIBC_V"           || unzip -qq "$DOWNLOAD_PATH/picolibc-$PICOLIBC_V.zip" -d "$BUILD_PATH"

if [ "$GMP_V" != "" ]; then
    test -f "$DOWNLOAD_PATH/gmp-$GMP_V.tar.bz2"      || download "https://ftpmirror.gnu.org/gnu/gmp/gmp-$GMP_V.tar.bz2"
    test -d "$BUILD_PATH/gmp-$GMP_V"                 || tar -xf "$DOWNLOAD_PATH/gmp-$GMP_V.tar.bz2" -C "$BUILD_PATH" # note: no .gz download file currently available
    pushd "$BUILD_PATH/gcc-$GCC_V"
    ln -sf ../"gmp-$GMP_V" "gmp"
    popd
fi

if [ "$MPC_V" != "" ]; then
    test -f "$DOWNLOAD_PATH/mpc-$MPC_V.tar.gz"       || download "https://ftpmirror.gnu.org/gnu/mpc/mpc-$MPC_V.tar.gz"
    test -d "$BUILD_PATH/mpc-$MPC_V"                 || tar -xzf "$DOWNLOAD_PATH/mpc-$MPC_V.tar.gz" -C "$BUILD_PATH"
    pushd "$BUILD_PATH/gcc-$GCC_V"
    ln -sf ../"mpc-$MPC_V" "mpc"
    popd
fi

if [ "$MPFR_V" != "" ]; then
    test -f "$DOWNLOAD_PATH/mpfr-$MPFR_V.tar.gz"     || download "https://ftpmirror.gnu.org/gnu/mpfr/mpfr-$MPFR_V.tar.gz"
    test -d "$BUILD_PATH/mpfr-$MPFR_V"               || tar -xzf "$DOWNLOAD_PATH/mpfr-$MPFR_V.tar.gz" -C "$BUILD_PATH"
    pushd "$BUILD_PATH/gcc-$GCC_V"
    ln -sf ../"mpfr-$MPFR_V" "mpfr"
    popd
fi

if [ "$MAKE_V" != "" ]; then
    test -f "$DOWNLOAD_PATH/make-$MAKE_V.tar.gz"     || download "https://ftpmirror.gnu.org/gnu/make/make-$MAKE_V.tar.gz"
    test -d "$BUILD_PATH/make-$MAKE_V"               || tar -xzf "$DOWNLOAD_PATH/make-$MAKE_V.tar.gz" -C "$BUILD_PATH"
fi

if [ "$ZLIB_V" != "" ]; then
    test -f "$DOWNLOAD_PATH/zlib-$ZLIB_V.tar.gz"     || download "https://zlib.net/fossils/zlib-$ZLIB_V.tar.gz"
    test -d "$BUILD_PATH/zlib-$ZLIB_V"               || tar -xzf "$DOWNLOAD_PATH/zlib-$ZLIB_V.tar.gz" -C "$BUILD_PATH"
fi

cd "$BUILD_PATH"

# Deduce build triplet using config.guess (if not specified)
# This is by the definition the current system so it should be OK.
if [ "$N64_BUILD" == "" ]; then
    N64_BUILD=$("binutils-$BINUTILS_V"/config.guess)
fi

if [ "$N64_HOST" == "" ]; then
    N64_HOST="$N64_BUILD"
fi


if [ "$N64_BUILD" == "$N64_HOST" ]; then
    # Standard cross.
    CROSS_PREFIX=$INSTALL_PATH
else
    # Canadian cross.
    # The standard BUILD->TARGET cross-compiler will be installed into a separate prefix, as it is not
    # part of the distribution.
    mkdir -p cross_prefix
    CROSS_PREFIX="$(cd "$(dirname -- "cross_prefix")" >/dev/null; pwd -P)/$(basename -- "cross_prefix")"
    PATH="$CROSS_PREFIX/bin:$PATH"
    export PATH

    # We need to build a canadian toolchain.
    # First we need a host compiler, that is binutils+gcc targeting the host. For instance,
    # when building a Libdragon Windows toolchain from Linux, this would be x86_64-w64-ming32,
    # that is, a compiler that we run that generates Windows executables.
    # Check if a host compiler is available. If so, we can just skip this step.
    if command_exists "$N64_HOST"-gcc; then
        echo Found host compiler: "$N64_HOST"-gcc in PATH. Using it.
    else
        if [ "$N64_HOST" == "x86_64-w64-mingw32" ]; then
            echo This script requires a working Windows cross-compiler.
            echo We could build it for you, but it would make the process even longer.
            echo Install it instead:
            echo "  * Linux (Debian/Ubuntu): apt install mingw-w64"
            echo "  * macOS: brew install mingw-w64"
            exit 1
        else
            echo "Unimplemented option: we support building a Windows toolchain only, for now."
        fi
    fi
fi

# Build zlib. This is only required on mingw, as all other systems do have
# zlib installed by default. So we only implement build process for mingw.
if [ "$ZLIB_V" != "" ]; then
    pushd "zlib-$ZLIB_V"
    if [ "$N64_HOST" = "x86_64-w64-mingw32" ]; then
        sed -e s/"PREFIX ="/"PREFIX = $N64_HOST-"/ -i win32/Makefile.gcc
        make -f win32/Makefile.gcc -j "$JOBS"
        (
            export \
                BINARY_PATH="$INSTALL_PATH/bin" \
                INCLUDE_PATH="$INSTALL_PATH/include" \
                LIBRARY_PATH="$INSTALL_PATH/lib"
            autosudo make -f win32/Makefile.gcc install
        )
    fi
    popd
fi

# Build GMP/MFPR and install them. This will be useful later for the gdb build,
# for mingw32 where those dependencies are not easily available.
if [ "$N64_HOST" = "x86_64-w64-mingw32" ]; then
    pushd "gmp-$GMP_V"
    ./configure \
        --prefix="$INSTALL_PATH" \
        --host="$N64_HOST" \
        --enable-static \
        --disable-shared
    make -j "$JOBS"
    autosudo make install-strip
    make distclean
    popd

    pushd "mpfr-$MPFR_V"
    ./configure \
        --prefix="$INSTALL_PATH" \
        --host="$N64_HOST" \
        --with-gmp="$INSTALL_PATH" \
        --enable-static \
        --disable-shared
    make -j "$JOBS"
    autosudo make install-strip
    make distclean
    popd
fi

# Compile BUILD->TARGET binutils
mkdir -p binutils_compile_target
pushd binutils_compile_target
../"binutils-$BINUTILS_V"/configure "${BINUTILS_CONFIGURE_ARGS[@]}" \
    --prefix="$CROSS_PREFIX" \
    --target="$N64_TARGET" \
    --with-cpu=mips64vr4300 \
    --disable-werror
make -j "$JOBS"
autosudo make install-strip
popd

# Compile GCC for MIPS N64.
# We need to build the C++ compiler to build the target libstd++ later.
mkdir -p gcc_compile_target
pushd gcc_compile_target
../"gcc-$GCC_V"/configure "${GCC_CONFIGURE_ARGS[@]}" \
    --prefix="$CROSS_PREFIX" \
    --target="$N64_TARGET" \
    --with-arch=vr4300 \
    --with-tune=vr4300 \
    --enable-languages=c,c++ \
    --without-headers \
    --disable-libssp \
    --enable-multilib \
    --disable-shared \
    --with-gcc \
    --with-newlib \
    --disable-win32-registry \
    --disable-nls \
    --disable-werror
make all-gcc -j "$JOBS"
autosudo make install-gcc
make all-target-libgcc -j "$JOBS"
autosudo make install-target-libgcc
popd

# Now check if we need to build a canadian toolchain.
if [ "$N64_BUILD" != "$N64_HOST" ]; then
    # Compile HOST->TARGET binutils
    # NOTE: we pass --without-msgpack to workaround a bug in Binutils, introduced
    # with this commit: https://sourceware.org/git/?p=binutils-gdb.git;a=commit;h=2952f10cd79af4645222f124f28c7928287d8113
    # This is due to the fact that pkg-config is used to activate compilation with msgpack
    # but that it is not correct in the case of a canadian cross.
    echo "Compiling binutils-$BINUTILS_V for foreign host"
    mkdir -p binutils_compile_host
    pushd binutils_compile_host
    ../"binutils-$BINUTILS_V"/configure \
        --prefix="$INSTALL_PATH" \
        --build="$N64_BUILD" \
        --host="$N64_HOST" \
        --target="$N64_TARGET" \
        --disable-werror \
        --without-msgpack
    make -j "$JOBS"
    autosudo make install-strip
    popd

    # Compile HOST->TARGET gcc
    mkdir -p gcc_compile
    pushd gcc_compile
    CFLAGS_FOR_TARGET="-O2" CXXFLAGS_FOR_TARGET="-O2" \
        ../"gcc-$GCC_V"/configure \
        --prefix="$INSTALL_PATH" \
        --target="$N64_TARGET" \
        --build="$N64_BUILD" \
        --host="$N64_HOST" \
        --disable-werror \
        --with-arch=vr4300 \
        --with-tune=vr4300 \
        --enable-languages=c,c++ \
        --with-newlib \
        --enable-multilib \
        --with-gcc \
        --disable-libssp \
        --disable-shared \
        --disable-win32-registry \
        --disable-nls
    make all-gcc -j "$JOBS"
    autosudo make install-gcc
    make all-target-libgcc -j "$JOBS"
    autosudo make install-target-libgcc
    popd
fi

# Patch the GCC installation that will be used for target libraries ($CROSS_PREIFX),
# to always use our "magic" ktls.h header. This is required to use TLS, which is
# used by picolibc.
install_ktls_header "$CROSS_PREFIX"
patch_gcc_specs 0

# Compile newlib for target
mkdir -p newlib_compile_target
pushd newlib_compile_target
CFLAGS_FOR_TARGET="-DHAVE_ASSERT_FUNC -O2 -fpermissive" ../"newlib-$NEWLIB_V"/configure \
    --prefix="$INSTALL_PATH" \
    --target="$N64_TARGET" \
    --with-cpu=mips64vr4300 \
    --disable-libssp \
    --disable-werror \
    --enable-newlib-io-c99-formats \
    --enable-newlib-multithread \
    --enable-newlib-retargetable-locking
make -j "$JOBS"
autosudo make install
popd

# Meson cross file (required to build picolibc). Materialize from inline block.
MESON_CROSS_FILE="$BUILD_PATH/meson-cross.txt"
awk -v start=": <<'__MESON_CROSS_BLOCK__'" -v end="__MESON_CROSS_BLOCK__" '
    $0 == start {capture=1; next}
    $0 == end {exit}
    capture {print}
' "$SCRIPT_PATH" > "$MESON_CROSS_FILE"

# Compile picolibc for target directly into final sysroot
rm -rf picolibc_compile_target
mkdir -p picolibc_compile_target
pushd picolibc_compile_target
meson setup \
    --cross-file="$MESON_CROSS_FILE" \
    -Dmultilib=false \
    -Dpicocrt=false \
    -Dpicolib=false \
    -Dsemihost=false \
    -Dspecsdir=none \
    -Dtests=false \
    -Dtinystdio=true \
    -Dfast-bufio=true \
    -Dio-long-long=true \
    -Dio-pos-args=true \
    -Dio-percent-b=true \
    -Dposix-console=true \
    -Dformat-default=double \
    -Dnewlib-fseek-optimization=false \
    -Dnewlib-fvwrite-in-streamio=false \
    -Dnewlib-io-float=false \
    -Dnewlib-stdio64=false \
    -Dnewlib-unbuf-stream-opt=false \
    -Dnewlib-nano-malloc=false \
    -Dthread-local-storage=true \
    -Dpicoexit=false \
    -Dprefix="$INSTALL_PATH" \
    -Dlibdir=mips64-elf/picolibc/lib \
    -Dincludedir=mips64-elf/picolibc/include \
    ../"picolibc-$PICOLIBC_V"
ninja -j "$JOBS"
autosudo ninja install
popd

# Finish building the target libraries (libstdc++, libsupc++, libatomic)
pushd gcc_compile_target
make all -j "$JOBS"
autosudo make install-strip
popd

# Patch again the spec files with relocatable include path for distribution.
# This time install into the final HOST->TARGET compiler (in case of canadian).
install_ktls_header "$INSTALL_PATH"
patch_gcc_specs 1

# On Linux/Mac, where symlinks are supported, relocate newlib into a subdirectory for symmetry
# with picolibc, and use a symlink to keep it as default for backward compatibility.
if [ "$N64_HOST" != "x86_64-w64-mingw32" ]; then
    if [ ! -L "$INSTALL_PATH/$N64_TARGET/include" ]; then
        autosudo mkdir -p "$INSTALL_PATH/$N64_TARGET/newlib"

        autosudo mv "$INSTALL_PATH/$N64_TARGET/include" "$INSTALL_PATH/$N64_TARGET/newlib/include"
        autosudo mv "$INSTALL_PATH/$N64_TARGET/lib"     "$INSTALL_PATH/$N64_TARGET/newlib/lib"

        autosudo ln -sfn "$INSTALL_PATH/$N64_TARGET/newlib/include" "$INSTALL_PATH/$N64_TARGET/include"
        autosudo ln -sfn "$INSTALL_PATH/$N64_TARGET/newlib/lib"     "$INSTALL_PATH/$N64_TARGET/lib"
    fi
else
    autosudo mkdir -p "$INSTALL_PATH/$N64_TARGET/newlib"

    autosudo cp -a "$INSTALL_PATH/$N64_TARGET/include" "$INSTALL_PATH/$N64_TARGET/newlib"
    autosudo cp -a "$INSTALL_PATH/$N64_TARGET/lib"     "$INSTALL_PATH/$N64_TARGET/newlib"
fi

# Write per-libc toolchain.version files under INSTALL_PATH
# Ensure include directories exist before writing
dest_dir="$INSTALL_PATH/$N64_TARGET/newlib/include"
autosudo mkdir -p "$dest_dir"
TOOLCHAIN_VERSION_FILE_NEWLIB="$dest_dir/toolchain.version"
VERSION_NEWLIB="{\n  \"host\": \"$N64_HOST\",\n  \"binutils\": \"$BINUTILS_V\",\n  \"gcc\": \"$GCC_V\",\n  \"newlib\": \"$NEWLIB_V\"\n}"
autosudo sh -c "printf '%s\\n' \"$VERSION_NEWLIB\" > \"$TOOLCHAIN_VERSION_FILE_NEWLIB\""

dest_dir="$INSTALL_PATH/$N64_TARGET/picolibc/include"
autosudo mkdir -p "$dest_dir"
TOOLCHAIN_VERSION_FILE_PICO="$dest_dir/toolchain.version"
VERSION_PICO="{\n  \"host\": \"$N64_HOST\",\n  \"binutils\": \"$BINUTILS_V\",\n  \"gcc\": \"$GCC_V\",\n  \"picolibc\": \"$PICOLIBC_V\"\n}"
autosudo sh -c "printf '%s\\n' \"$VERSION_PICO\" > \"$TOOLCHAIN_VERSION_FILE_PICO\""

if [ "$MAKE_V" != "" ]; then
    pushd "make-$MAKE_V"
    ./configure \
        --prefix="$INSTALL_PATH" \
        --disable-largefile \
        --disable-nls \
        --disable-rpath \
        --build="$N64_BUILD" \
        --host="$N64_HOST"
    make -j "$JOBS"
    autosudo make install-strip
    popd
fi

# Final message
set +x
echo
echo "***********************************************"
echo "Libdragon toolchain correctly built and installed"
echo "Installation directory: \"${N64_INST}\""
echo "Build directory: \"${BUILD_PATH}\" (can be removed now)"
echo "If you would like to install GDB in your toolchain, run build-gdb.sh"

: <<'__KTLS_H_BLOCK__'
#pragma once
#ifndef __ASSEMBLER__
__asm__ (
    ".ifndef __RDHWR_WAS_DEFINED" "\n"
    ".macro rdhwr rt, rd" "\n"
    "    lw \\rt, %gprel(__th_cur_tp)($gp)" "\n"
    ".endm"               "\n"
    ".set __RDHWR_WAS_DEFINED, 1" "\n"
    ".endif" "\n"
);
#endif
__KTLS_H_BLOCK__

: <<'__MESON_CROSS_BLOCK__'
[binaries]
c = 'mips64-elf-gcc'
ar = 'mips64-elf-ar'
as = 'mips64-elf-as'
nm = 'mips64-elf-nm'
strip = 'mips64-elf-strip'

[host_machine]
system = 'none'
cpu_family = 'mips64'
cpu = 'mips64vr4300'
endian = 'big'

[properties]
skip_sanity_check = true

[built-in options]
c_args = [ '-falign-functions=32' ]
__MESON_CROSS_BLOCK__
