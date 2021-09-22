#!/usr/bin/env bash

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

makeWithInclude(){
  make -j${CPU_COUNT} -I${N64_INST}/include $@
}

# Limit the number of make jobs to the number of CPUs
CPU_COUNT=$(nproc)

# Specify where to get libmikmod from and where to put it
LIBMIKMOD_REPO=https://github.com/networkfusion/libmikmod.git
LIBMIKMOD_COMMIT=738b1e8b11b470360b1b919680d1d88429d9d174
LIBMIKMOD_DIR=/tmp/libmikmod

# Clean, build, and install libdragon + tools
makeWithInclude clobber
makeWithInclude install tools-install

# Remove the cloned libmikmod repo if it already exists
[ -d "$LIBMIKMOD_DIR" ] && rm -Rf $LIBMIKMOD_DIR
# Clone, compile, and install libmikmod
git clone $LIBMIKMOD_REPO $LIBMIKMOD_DIR
pushd $LIBMIKMOD_DIR/n64
git checkout $LIBMIKMOD_COMMIT
makeWithInclude
makeWithInclude install
popd
rm -Rf $LIBMIKMOD_DIR

# Build all of the libdragon examples
makeWithInclude examples
