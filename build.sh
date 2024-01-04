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

# Clean, build, and install libdragon + tools
sudoMakeWithParams install-mk
makeWithParams clobber
makeWithParams libdragon tools
sudoMakeWithParams install tools-install

# Build examples and tests - libdragon must be already installed at this point,
# so first clobber the build to make sure that everything works against the
# installed version rather than using local artifacts.
makeWithParams clobber
makeWithParams examples
makeWithParams test

echo
echo Libdragon built successfully!
