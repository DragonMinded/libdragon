#! /bin/bash
# N64 MIPS GDB toolchain build/install script for Unix distributions
# (c) DragonMinded and libDragon Contributors.
# See the root folder for license information.

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

# Check that N64_INST is defined
if [ -z "${N64_INST-}" ]; then
    echo "N64_INST environment variable is not defined."
    echo "Please define N64_INST and point it to the requested installation directory"
    exit 1
fi

# Dependency source libs (Versions)
GDB_V=16.2

# Defines the build system variables to allow cross compilation.
N64_HOST=${N64_HOST:-""}
N64_TARGET=${N64_TARGET:-mips64-elf}

# Set N64_INST before calling the script to change the default installation directory path
INSTALL_PATH="${N64_INST}"
# Path where the toolchain will be built.
BUILD_PATH="${BUILD_PATH:-toolchain}"
DOWNLOAD_PATH="${DOWNLOAD_PATH:-$BUILD_PATH}"

# Determine how many parallel Make jobs to run based on CPU count
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

# GDB configure arguments to use system GMP/MPC/MFPF
GDB_CONFIGURE_ARGS=()

# Resolve absolute paths for build and download directories
BUILD_PATH=$(cd "$BUILD_PATH" && pwd)
DOWNLOAD_PATH=$(cd "$DOWNLOAD_PATH" && pwd)

# Check if a command-line tool is available: status 0 means "yes"; status 1 means "no"
command_exists () {
    (command -v "$1" >/dev/null 2>&1)
    return $?
}

# Download the file URL using wget or curl (depending on which is installed)
download () {
    local url="$1"
    local file="$DOWNLOAD_PATH/$(basename "$url")"
    local tmpfile="$file.part"
    if   command_exists wget ; then wget --continue --output-document "$tmpfile" "$url"
    elif command_exists curl ; then curl --location --output "$tmpfile" "$url"
    else
        echo "Install wget or curl to download toolchain sources" 1>&2
        return 1
    fi
    mv "$tmpfile" "$file"
}

# Dependency downloads and unpack
test -f "$DOWNLOAD_PATH/gdb-$GDB_V.tar.gz" || download "https://ftp.gnu.org/gnu/gdb/gdb-$GDB_V.tar.gz"
test -d "$BUILD_PATH/gdb-$GDB_V"           || tar -xzf "$DOWNLOAD_PATH/gdb-$GDB_V.tar.gz" -C "$BUILD_PATH"

# Resolve dependencies on macOS via homebrew
if [[ $OSTYPE == 'darwin'* ]]; then
    # Tell GDB configure to use Homebrew's GMP, MPFR, MPC, and Zlib.
    # These should have already been installed by build-toolchain.sh
    GDB_CONFIGURE_ARGS=(
        "--with-gmp=$(brew --prefix)"
        "--with-mpfr=$(brew --prefix)"
        "--with-mpc=$(brew --prefix)"
        "--with-zlib=$(brew --prefix)"
    )
else
    # Configure GDB arguments for non-macOS platforms
    GDB_CONFIGURE_ARGS+=("--with-system-zlib")
fi

# Add host to GDG configure arguments if defined
if [[ -n "${N64_HOST}" ]]; then
    GDB_CONFIGURE_ARGS+=("--host=${N64_HOST}")
fi

# Add target to GDB configure arguments if defined
if [[ -n "${N64_TARGET}" ]]; then
    GDB_CONFIGURE_ARGS+=("--target=${N64_TARGET}")
fi

# Compile GDB
pushd "$BUILD_PATH/gdb-$GDB_V"
./configure "${GDB_CONFIGURE_ARGS[@]}" \
    --prefix="$INSTALL_PATH" \
    --disable-docs \
    --disable-gdbserver \
    --disable-binutils \
    --disable-gas \
    --disable-sim \
    --disable-gprof \
    --disable-inprocess-agent
make all -j "$JOBS"
make install-strip || sudo make install-strip || su -c "make install-strip"
popd

# Final message
echo
echo "***********************************************"
echo "GDB correctly built and installed to LibDragon toolchain"
echo "Installation directory: \"${N64_INST}\""
echo "Build directory: \"${BUILD_PATH}\" (can be removed now)"
