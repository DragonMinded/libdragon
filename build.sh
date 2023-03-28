#!/usr/bin/env bash

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

if [[ -z ${N64_INST-} ]]; then
  echo N64_INST environment variable is not defined
  echo Please set N64_INST to point to your libdragon toolchain directory
  exit 1
fi

if [[ $OSTYPE == 'darwin'* ]]; then
  if command -v brew >/dev/null; then
    brew install libpng
    CFLAGS="-I$(brew --prefix)/include"
    LDFLAGS="-L$(brew --prefix)/lib"
  fi
fi

CFLAGS=${CFLAGS:-}; export CFLAGS
LDFLAGS=${LDFLAGS:-}; export LDFLAGS

makeWithParams(){
  make -j"${JOBS}" "$@" || \
    sudo env N64_INST="$N64_INST" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
      make -j"${JOBS}" "$@"
}

# Limit the number of make jobs to the number of CPUs
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

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
makeWithParams test
