#! /bin/bash
# N64 MIPS GCC toolchain build/install script for Unix distributions
# (c) 2012-2021 DragonMinded and libDragon Contributors.
# See the root folder for license information.

# Before calling this script, make sure you have GMP, MPFR and TexInfo
# packages installed in your system.  On a Debian-based system this is
# achieved by typing the following commands:
#
# sudo apt-get install libmpfr-dev
# sudo apt-get install texinfo
# sudo apt-get install libmpc-dev

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

# Check that N64_INST is defined
if [ -z "${N64_INST-}" ]; then
  echo "N64_INST environment variable is not defined."
  echo "Please define N64_INST and point it to the requested installation directory"
  exit 1
fi

# Path where the toolchain will be built.
BUILD_PATH="${BUILD_PATH:-toolchain}"

# Set N64_INST before calling the script to change the default installation directory path
INSTALL_PATH="${N64_INST}"
# Set PATH for newlib to compile using GCC for MIPS N64 (pass 1)
export PATH="$PATH:$INSTALL_PATH/bin"

# Determine how many parallel Make jobs to run based on CPU count
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

# Additional GCC configure arguments
GCC_CONFIGURE_ARGS=()

# Dependency source libs (Versions)
BINUTILS_V=2.39
GCC_V=12.2.0
NEWLIB_V=4.2.0.20211231

# Check if a command-line tool is available: status 0 means "yes"; status 1 means "no"
command_exists () {
  (command -v "$1" >/dev/null 2>&1)
  return $?
}

# Download the file URL using wget or curl (depending on which is installed)
download () {
  if   command_exists wget ; then wget -c  "$1"
  elif command_exists curl ; then curl -LO "$1"
  else
    echo "Install wget or curl to download toolchain sources" 1>&2
    return 1
  fi
}

# Compilation on macOS via homebrew
if [[ $OSTYPE == 'darwin'* ]]; then
  if ! command_exists brew; then
    echo "Compilation on macOS is supported via Homebrew (https://brew.sh)"
    echo "Please install homebrew and try again"
    exit 1
  fi

  # Install required dependencies
  brew install -q gmp mpfr libmpc gsed

  # Tell GCC configure where to find the dependent libraries
  GCC_CONFIGURE_ARGS=(
    "--with-gmp=$(brew --prefix)"
    "--with-mpfr=$(brew --prefix)"
    "--with-mpc=$(brew --prefix)"
  )

  # Install GNU sed as default sed in PATH. GCC compilation fails otherwise,
  # because it does not work with BSD sed.
  export PATH="$(brew --prefix gsed)/libexec/gnubin:$PATH"
fi

# Create build path and enter it
mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

# Dependency source: Download stage
test -f "binutils-$BINUTILS_V.tar.gz" || download "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_V.tar.gz"
test -f "gcc-$GCC_V.tar.gz"           || download "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_V/gcc-$GCC_V.tar.gz"
test -f "newlib-$NEWLIB_V.tar.gz"     || download "https://sourceware.org/pub/newlib/newlib-$NEWLIB_V.tar.gz"

# Dependency source: Extract stage
test -d "binutils-$BINUTILS_V" || tar -xzf "binutils-$BINUTILS_V.tar.gz"
test -d "gcc-$GCC_V"           || tar -xzf "gcc-$GCC_V.tar.gz"
test -d "newlib-$NEWLIB_V"     || tar -xzf "newlib-$NEWLIB_V.tar.gz"

# Compile binutils
cd "binutils-$BINUTILS_V"
./configure \
  --prefix="$INSTALL_PATH" \
  --target=mips64-elf \
  --with-cpu=mips64vr4300 \
  --disable-werror
make -j "$JOBS"
make install-strip || sudo make install-strip || su -c "make install-strip"

# Compile GCC for MIPS N64 (pass 1) outside of the source tree
cd ..
rm -rf gcc_compile
mkdir gcc_compile
cd gcc_compile
../"gcc-$GCC_V"/configure "${GCC_CONFIGURE_ARGS[@]}" \
  --prefix="$INSTALL_PATH" \
  --target=mips64-elf \
  --with-arch=vr4300 \
  --with-tune=vr4300 \
  --enable-languages=c \
  --without-headers \
  --with-newlib \
  --disable-libssp \
  --enable-multilib \
  --disable-shared \
  --with-gcc \
  --disable-threads \
  --disable-win32-registry \
  --disable-nls \
  --disable-werror \
  --with-system-zlib
make all-gcc -j "$JOBS"
make all-target-libgcc -j "$JOBS"
make install-strip-gcc || sudo make install-strip-gcc || su -c "make install-strip-gcc"
make install-target-libgcc || sudo make install-target-libgcc || su -c "make install-target-libgcc"

# Compile newlib
cd ../"newlib-$NEWLIB_V"
CFLAGS_FOR_TARGET="-DHAVE_ASSERT_FUNC -O2" ./configure \
  --target=mips64-elf \
  --prefix="$INSTALL_PATH" \
  --with-cpu=mips64vr4300 \
  --disable-threads \
  --disable-libssp \
  --disable-werror
make -j "$JOBS"
make install || sudo env PATH="$PATH" make install || su -c "env PATH=\"$PATH\" make install"

# Compile GCC for MIPS N64 (pass 2) outside of the source tree
cd ..
rm -rf gcc_compile
mkdir gcc_compile
cd gcc_compile
CFLAGS_FOR_TARGET="-O2" CXXFLAGS_FOR_TARGET="-O2" \
  ../"gcc-$GCC_V"/configure "${GCC_CONFIGURE_ARGS[@]}" \
  --prefix="$INSTALL_PATH" \
  --target=mips64-elf \
  --with-arch=vr4300 \
  --with-tune=vr4300 \
  --enable-languages=c,c++ \
  --with-newlib \
  --disable-libssp \
  --enable-multilib \
  --disable-shared \
  --with-gcc \
  --disable-threads \
  --disable-win32-registry \
  --disable-nls \
  --with-system-zlib
make -j "$JOBS"
make install-strip || sudo make install-strip || su -c "make install-strip"

# Final message
echo
echo "***********************************************"
echo "Libdragon toolchain correctly built and installed"
echo "Installation directory: \"${N64_INST}\""
echo "Build directory: \"${BUILD_PATH}\" (can be removed now)"
