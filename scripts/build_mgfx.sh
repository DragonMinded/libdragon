#!/usr/bin/env bash
#
# This script invokes RSPL to compile all variants of the mgfx vertex shader.

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

if [[ -z ${RSPL_INST-} ]]; then
  echo RSPL_INST environment variable is not defined
  echo Please set RSPL_INST to point to the RSPL root directory
  exit 1
fi

RSPL=${RSPL_INST}/dist/cli.mjs
MGFX_DIR=src/magma/
MGFX_RSPL=${MGFX_DIR}/rsp_mgfx.rspl

node ${RSPL} ${MGFX_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -o ${MGFX_DIR}/rsp_mgfx.S
node ${RSPL} ${MGFX_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -D ENABLE_ENV_MAP -o ${MGFX_DIR}/rsp_mgfx_env.S
