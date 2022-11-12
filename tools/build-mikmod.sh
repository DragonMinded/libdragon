#!/usr/bin/env bash

# This script downloads and build MikMod, a music library for playing
# different module files. Notice that, albeit ported to N64, it is a
# CPU-only port, so it will use lots of CPU time to play the music.
# This is basically kept here for backward compatibility for old code
# using it. New code should default to use the new mixer library
# with its XM64/WAV64 support for music files.

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

makeWithParams(){
  make -j"${JOBS}" "$@"
}

sudoMakeWithParams(){
  make -j"${JOBS}" "$@" || \
    sudo env N64_INST="$N64_INST" \
      make -j"${JOBS}" "$@"
}

# Limit the number of make jobs to the number of CPUs
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN)}"
JOBS="${JOBS:-1}" # If getconf returned nothing, default to 1

# Specify where to get libmikmod from and where to put it
LIBMIKMOD_REPO=https://github.com/networkfusion/libmikmod.git
LIBMIKMOD_COMMIT=738b1e8b11b470360b1b919680d1d88429d9d174
LIBMIKMOD_DIR=/tmp/libmikmod

# Remove the cloned libmikmod repo if it already exists
[ -d "$LIBMIKMOD_DIR" ] && rm -Rf $LIBMIKMOD_DIR
# Clone, compile, and install libmikmod
git clone $LIBMIKMOD_REPO $LIBMIKMOD_DIR
pushd $LIBMIKMOD_DIR/n64
git checkout $LIBMIKMOD_COMMIT
makeWithParams
sudoMakeWithParams install
popd
rm -Rf $LIBMIKMOD_DIR
