#!/usr/bin/env bash

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

# If --test is passed, run tests after building
TEST_MODE=false
while (( $# )); do
  case "$1" in
    --test) TEST_MODE=true; shift ;;
    *) break ;;
  esac
done

if [[ -z ${N64_INST-} ]]; then
  echo N64_INST environment variable is not defined
  echo Please set N64_INST to point to your libdragon toolchain directory
  exit 1
fi

# Msys identifies itself via the OSTYPE variable as msys (before Feb 2025)
# and "cygwin" (after Feb 2025). We want to detect both.
if [[ $OSTYPE == 'msys'* || $OSTYPE == 'cygwin'* ]]; then
  if [ "${MSYSTEM:-}" != "MINGW64" ]; then
    # We only support building host tools via mingw-x64 at the moment, so
    # enforce that to help users during installation.
    echo This script must be run from the \"MSYS2 MinGW x64\" shell
    echo Plase open that shell and run it again from there
    exit 1
  fi
  # Check if N64_INST contains backslashes, which is not supported by the build system
  if [[ "$N64_INST" == *\\* ]]; then
    echo "N64_INST contains backslashes, which is not supported by the build system."
    echo "Please set N64_INST to use forward slashes instead."
    exit 1
  fi
fi

# Check if ccache is installed, and if so, use it
if command -v ccache &> /dev/null; then
  export CCACHE=ccache
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

if [ "$TEST_MODE" = true ]; then
  # Run tests if --test was passed
  makeWithParams test
fi

# Build examples - libdragon must be already installed at this point,
# so first clobber the build to make sure that everything works against the
# installed version rather than using local artifacts.
makeWithParams clobber
makeWithParams examples

echo
echo Libdragon built successfully!
