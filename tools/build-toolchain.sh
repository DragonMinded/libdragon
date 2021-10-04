#! /bin/bash
# N64 MIPS GCC toolchain build/install script for Unix distributions
# (c) 2012-2021 Shaun Taylor and libDragon Contributors.
# See the root folder for license information.



# Before calling this script, make sure you have GMP, MPFR and TexInfo
# packages installed in your system.  On a Debian-based system this is
# achieved by typing the following commands:
#
# sudo apt-get install libmpfr-dev
# sudo apt-get install texinfo
# sudo apt-get install libmpc-dev

# Exit on error
set -e

# Set N64_INST before calling the script to change the default installation directory path
export INSTALL_PATH=${N64_INST:-/usr/local}

export JOBS=${JOBS:-`getconf _NPROCESSORS_ONLN`}
export JOBS=${JOBS:-1} # If getconf returned nothing, default to 1
export FORCE_DEFAULT_GCC=${FORCE_DEFAULT_GCC:-false}

# Set up path for newlib to compile later
export PATH=$PATH:$INSTALL_PATH/bin

# Dependency source libs (Versions)
export BINUTILS_V=2.36.1
export GCC_V=10.2.0
export NEWLIB_V=4.1.0

# Dependency source: Download stage
test -f binutils-$BINUTILS_V.tar.gz || wget -c https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_V.tar.gz
test -f gcc-$GCC_V.tar.gz || wget -c https://ftp.gnu.org/gnu/gcc/gcc-$GCC_V/gcc-$GCC_V.tar.gz
test -f newlib-$NEWLIB_V.tar.gz || wget -c https://sourceware.org/pub/newlib/newlib-$NEWLIB_V.tar.gz

# Dependency source: Extract stage
test -d binutils-$BINUTILS_V || tar -xzf binutils-$BINUTILS_V.tar.gz
test -d gcc-$GCC_V || tar -xzf gcc-$GCC_V.tar.gz
test -d newlib-$NEWLIB_V || tar -xzf newlib-$NEWLIB_V.tar.gz

# Binutils and newlib support compiling in source directory, GCC does not
rm -rf gcc_compile
mkdir gcc_compile

# Compile binutils
cd binutils-$BINUTILS_V
./configure \
  --prefix=${INSTALL_PATH} \
  --target=mips64-elf \
  --with-cpu=mips64vr4300 \
  --disable-werror
make -j$JOBS
make install || sudo make install || su -c "make install"

# Compile GCC for MIPS N64 (pass 1)
cd ../gcc_compile
../gcc-$GCC_V/configure \
  --prefix=${INSTALL_PATH} \
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
make all-gcc -j$JOBS
make all-target-libgcc -j$JOBS
make install-gcc || sudo make install-gcc || su -c "make install-gcc"
make install-target-libgcc || sudo make install-target-libgcc || su -c "make install-target-libgcc"

# Compile newlib
cd ../newlib-$NEWLIB_V
CFLAGS_FOR_TARGET="-DHAVE_ASSERT_FUNC" ./configure \
  --target=mips64-elf \
  --prefix=${INSTALL_PATH} \
  --with-cpu=mips64vr4300 \
  --disable-threads \
  --disable-libssp \
  --disable-werror
make -j$JOBS
make install || sudo env PATH="$PATH" make install || su -c "env PATH=$PATH make install"

# Compile GCC for MIPS N64 (pass 2)
cd ..
rm -rf gcc_compile
mkdir gcc_compile
cd gcc_compile
CFLAGS_FOR_TARGET="-G0 -O2" CXXFLAGS_FOR_TARGET="-G0 -O2" ../gcc-$GCC_V/configure \
  --prefix=${INSTALL_PATH} \
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
make -j$JOBS
make install || sudo make install || su -c "make install"
