#! /bin/bash
# N64 toolchain install script for Unix distributions by Shaun Taylor.
# Tested working on Ubuntu 18.04.

# You may be prompted for your password for installation steps.
# If the script tells you that writing failed on chksum64 and n64tool,
# this does not mean it failed.  The script simply retried with sudo
# and your password was cached.

# Before calling this script, make sure you have GMP, MPFR and TexInfo
# packages installed in your system.  On a Debian-based system this is
# achieved by typing the following commands:
#
# sudo apt-get install libmpfr-dev
# sudo apt-get install texinfo
# sudo apt-get install libmpc-dev

source ./config
bash ./download

test -d isl-$ISL_V                  || tar $ISL_T isl-$ISL_V.$ISL_E
test -d gmp-$GMP_V                  || tar $GMP_T gmp-$GMP_V.$GMP_E
test -d mpc-$MPC_V                  || tar $MPC_T mpc-$MPC_V.$MPC_E
test -d mpfr-$MPFR_V                || tar $MPFR_T mpfr-$MPFR_V.$MPFR_E

# Extract binutils
test -d binutils-$BINUTILS_V        || tar $BINUTILS_T binutils-$BINUTILS_V.$BINUTILS_E
pushd binutils-$BINUTILS_V          || echo "ERROR: extract binutils - pushd binutils-${BINUTILS_V} failed!"
test -L isl                         || ln -s ../isl-$ISL_V.$ISL_E isl
test -L gmp                         || ln -s ../gmp-$GMP_V.$GMP_E gmp
test -L mpc                         || ln -s ../mpc-$MPC_V.$MPC_E mpc
test -L mpfr                        || ln -s ../mpfr-$MPFR_V.$MPFR_E mpfr
popd                                || echo "ERROR: popd failed!"

# Extract gcc
test -d gcc-$GCC_V                  || tar $GCC_T gcc-$GCC_V.$GCC_E
pushd gcc-$GCC_V                    || echo "ERROR: extract gcc - pushd gcc-${GCC_V} failed!"
test -L isl                         || ln -s ../isl-$ISL_V.$ISL_E isl
test -L gmp                         || ln -s ../gmp-$GMP_V.$GMP_E gmp
test -L mpc                         || ln -s ../mpc-$MPC_V.$MPC_E mpc
test -L mpfr                        || ln -s ../mpfr-$MPFR_V.$MPFR_E mpfr
popd                                || echo "ERROR: popd failed!"

# Extract newlib
test -d newlib-$NEWLIB_V            || tar $NEWLIB_T newlib-$NEWLIB_V.$NEWLIB_E
pushd newlib-$NEWLIB_V              || echo "ERROR: extract pushd newlib-$NEWLIB_V"
test -L isl                         || ln -s ../isl-$ISL_V.$ISL_E isl
test -L gmp                         || ln -s ../gmp-$GMP_V.$GMP_E gmp
test -L mpc                         || ln -s ../mpc-$MPC_V.$MPC_E mpc
test -L mpfr                        || ln -s ../mpfr-$MPFR_V.$MPFR_E mpfr
popd                                || echo "ERROR: extract popd failed!"

# Extract make
test -d make-$MAKE_V                || tar $MAKE_T make-$MAKE_V.$MAKE_E

# Extract gdb
test -d gdb-$GDB_V                  || tar $GDB_T gdb-$GDB_V.$GDB_E
pushd gdb-$GDB_V                    || echo "ERROR: extract gdb - pushd $GDB_V failed!"
test -L isl                         || ln -s ../isl-$ISL_V.$ISL_E isl
test -L gmp                         || ln -s ../gmp-$GMP_V.$GMP_E gmp
test -L mpc                         || ln -s ../mpc-$MPC_V.$MPC_E mpc
test -L mpfr                        || ln -s ../mpfr-$MPFR_V.$MPFR_E mpfr
popd                                || echo "ERROR: extract gdb - popd failed!"

# Use same native GCC
if [ ! $(gcc --version | grep $GCC_V) ] && [ $FORCE_DEFAULT_GCC == 'false' ]; then
    rm -rf build_binutils
    mkdir build_binutils

    # Compile binutils
    cd build_binutils
    ../binutils-$BINUTILS_V/configure --prefix=${INSTALL_PATH}/gcc-$GCC_V --disable-werror
    make -j$JOBS
    make install || sudo make install || su -c "make install"

    # Compile gcc (native)
    cd ..
    rm -rf build_gcc
    mkdir build_gcc
    cd build_gcc
    ../gcc-$GCC_V/configure --prefix=${INSTALL_PATH}/gcc-$GCC_V --enable-languages=c,c++ --disable-nls
    make -j$JOBS
    make install || sudo make install || su -c "make install"

    # Use new compiler
    export PATH="${INSTALL_PATH}/gcc-$GCC_V/bin:${PATH}"
    export COMPILED_CUSTOM_NATIVE_GCC="true"
    cd ..
fi

# Binutils and newlib support compiling in source directory, GCC does not
rm -rf gcc_compile
mkdir gcc_compile

# Compile binutils
cd binutils-$BINUTILS_V
./configure --prefix=${INSTALL_PATH} --target=mips64-elf --with-cpu=mips64vr4300 --disable-werror --enable-plugins --enable-shared --enable-gold --enable-ld=default --enable-static --enable-nls --enable-64-bit-bfd --enable-multilib --enable-multitarget
make -j$JOBS
make install || sudo make install || su -c "make install"

# Compile gcc (pass 1)
cd ../gcc_compile
../gcc-$GCC_V/configure --prefix=${INSTALL_PATH} --target=mips64-elf --with-arch=vr4300 --with-tune=vr4300 --enable-languages=c --without-headers --with-newlib --disable-libssp --enable-multilib --enable-shared --with-gcc --enable-threads --disable-win32-registry --enable-nls --disable-werror --with-system-zlib --enable-lto --enable-static --enable-plugin --enable-gold --enable-libatomic --enable-libitm  --enable-libvtv
make all-gcc -j$JOBS
make all-target-libgcc -j$JOBS
make install-gcc || sudo make install-gcc || su -c "make install-gcc"
make install-target-libgcc || sudo make install-target-libgcc || su -c "make install-target-libgcc"

# Compile newlib
cd ../newlib-$NEWLIB_V
CFLAGS_FOR_TARGET="-DHAVE_ASSERT_FUNC" ./configure --target=mips64-elf --prefix=${INSTALL_PATH} --with-cpu=mips64vr4300 --enable-threads --disable-libssp --disable-werror
make -j$JOBS
make install || sudo env PATH="$PATH" make install || su -c "env PATH=$PATH make install"

# Compile gcc (pass 2)
cd ..
rm -rf gcc_compile
mkdir gcc_compile
cd gcc_compile
CFLAGS_FOR_TARGET="-G0 -O2" CXXFLAGS_FOR_TARGET="-G0 -O2" ../gcc-$GCC_V/configure --prefix=${INSTALL_PATH} --target=mips64-elf --with-arch=vr4300 --with-tune=vr4300 --enable-languages=c,c++ --with-newlib --disable-libssp --enable-multilib --enable-shared --with-gcc --enable-threads --disable-win32-registry --disable-nls --with-system-zlib --enable-lto --enable-plugin --enable-static --enable-gold --enable-libatomic --enable-libitm --enable-libvtv
make -j$JOBS
make install || sudo make install || su -c "make install"

if [ "$COMPILED_CUSTOM_NATIVE_GCC" == "true" ]; then
    rm -rf ${INSTALL_PATH}/gcc-$GCC_V
fi
