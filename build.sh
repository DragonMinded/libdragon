#!/usr/bin/env bash

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

makeWithParams(){
  make -j${CPU_COUNT} $@
}

# Limit the number of make jobs to the number of CPUs
CPU_COUNT=$(nproc)

# Specify where to get libmikmod from and where to put it
LIBMIKMOD_REPO=https://github.com/networkfusion/libmikmod.git
LIBMIKMOD_COMMIT=738b1e8b11b470360b1b919680d1d88429d9d174
LIBMIKMOD_DIR=/tmp/libmikmod

# Clean, build, and install libdragon + tools
makeWithParams clobber
makeWithParams install tools-install

# Remove the cloned libmikmod repo if it already exists
[ -d "$LIBMIKMOD_DIR" ] && rm -Rf $LIBMIKMOD_DIR
# Clone, compile, and install libmikmod
git clone $LIBMIKMOD_REPO $LIBMIKMOD_DIR
pushd $LIBMIKMOD_DIR/n64
git checkout $LIBMIKMOD_COMMIT
makeWithParams
makeWithParams install
popd
rm -Rf $LIBMIKMOD_DIR

# Build examples and tests - libdragon must be already installed at this point,
# so first clobber the build to make sure that everything works against the
# installed version rather than using local artifacts.
makeWithParams clobber
makeWithParams examples
makeWithParams -C ./tests
