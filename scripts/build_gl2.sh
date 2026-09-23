#!/usr/bin/env bash
#
# This script invokes RSPL to compile rsp_gl2.rspl.

# Bash strict mode http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

if [[ -z ${RSPL_INST-} ]]; then
  echo RSPL_INST environment variable is not defined
  echo Please set RSPL_INST to point to the RSPL root directory
  exit 1
fi

RSPL=${RSPL_INST}/dist/cli.mjs
GL_DIR=src/GL/
GL2_RSPL=${GL_DIR}/rsp_gl2.rspl

node ${RSPL} ${GL2_RSPL} --patch GLCmd_PreInitMagma --reorder --opt-time=60 --opt-worker=16
