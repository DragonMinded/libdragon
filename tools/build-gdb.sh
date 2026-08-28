#! /bin/bash
# N64 MIPS GDB toolchain build/install script for Unix distributions
# (c) DragonMinded and libDragon Contributors.
# See the root folder for license information.

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
exec > >(tee "$BUILD_PATH/build-gdb.log") 2>&1
echo "Build started at: $(date)"

# Dependency source libs (Versions)
GDB_V=16.2

# Defines the build system variables to allow cross compilation.
N64_HOST=${N64_HOST:-""}
N64_TARGET=${N64_TARGET:-mips64-elf}

# Set N64_INST before calling the script to change the default installation directory path
INSTALL_PATH="${N64_INST}"

# Determine how many parallel Make jobs to run based on CPU count
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

# GDB configure arguments to use system GMP/MPC/MFPF
GDB_CONFIGURE_ARGS=()

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

find_gnumirror () {
    local gnu_mirrors=()
    readarray -t gnu_mirrors < gnumirrors.txt
    local timeout=10
    local url response_time
    local best_url=''
    local best_response_time=100
    local exit_code

    for url in "${gnu_mirrors[@]}"; do
        if command_exists wget ; then
            local start_time=$(date +%s.%N)
            if wget --spider --quiet --timeout="$timeout" --tries=1 "$url" ; then
                exit_code=0
            else
                exit_code=1
            fi
            local end_time=$(date +%s.%N)
            response_time=$(awk "BEGIN {printf \"%.3f\", $end_time - $start_time}")
        elif command_exists curl ; then
            response_time=$(
                curl -s -o /dev/null -I -w '%{time_total}' --max-time "$timeout" "$url"
            )
            exit_code=$?
        else
            echo "Install wget or curl to download toolchain sources" 1>&2
            return 1
        fi
        if [[ $exit_code -eq 0 ]]; then
            if (( $(echo $response_time $best_response_time | awk '{if ($1 < $2) print 1;}') )); then
                best_url=$url
                best_response_time=$response_time
                timeout=$best_response_time
            fi
        fi
    done
    if [ -z "${best_url}" ]; then
        echo "No gnu mirror found (are you online?)" 1>&2
        return 1
    fi
    GNU_MIRROR=$best_url
}

download_gnumirror () {
    local urlpath="$1"
    if [ -z "${GNU_MIRROR-}" ]; then
        find_gnumirror
    fi
    download "$GNU_MIRROR/$urlpath"
}

# Dependency downloads and unpack
test -f "$DOWNLOAD_PATH/gdb-$GDB_V.tar.gz" || download_gnumirror "gdb/gdb-$GDB_V.tar.gz"
test -d "$BUILD_PATH/gdb-$GDB_V"           || tar -xzf "$DOWNLOAD_PATH/gdb-$GDB_V.tar.gz" -C "$BUILD_PATH"

# Resolve dependencies on macOS via homebrew
if [[ $OSTYPE == 'darwin'* ]]; then
    # Tell GDB configure to use Homebrew's Python, GMP, MPFR, MPC, and Zlib.
    # These should have already been installed by build-toolchain.sh
    GDB_CONFIGURE_ARGS=(
        "--with-gmp=$(brew --prefix gmp)"
        "--with-mpfr=$(brew --prefix mpfr)"
        "--with-mpc=$(brew --prefix libmpc)"
        "--with-isl=$(brew --prefix isl)"
        "--with-python=$(brew --prefix python3)/bin/python3"
        "--with-system-zlib"
    )
elif [ "$N64_HOST" == "x86_64-w64-mingw32" ]; then
    # Configure GDB arguments for Windows cross-compilation
    GDB_CONFIGURE_ARGS+=("--with-zlib=$N64_INST")
    GDB_CONFIGURE_ARGS+=("--with-gmp=$N64_INST")
    GDB_CONFIGURE_ARGS+=("--with-mpfr=$N64_INST")
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
set +x
echo
echo "***********************************************"
echo "GDB correctly built and installed to LibDragon toolchain"
echo "Installation directory: \"${N64_INST}\""
echo "Build directory: \"${BUILD_PATH}\" (can be removed now)"
