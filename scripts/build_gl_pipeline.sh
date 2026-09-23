#!/usr/bin/env bash
#
# This script invokes RSPL to compile all variants of the gl pipeline vertex shader.

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
GL_PIPELINE_RSPL=${GL_DIR}/rsp_gl_pipeline.rspl

node ${RSPL} ${GL_PIPELINE_RSPL_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -o ${GL_DIR}/rsp_gl_pipeline.S
node ${RSPL} ${GL_PIPELINE_RSPL_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -D ENABLE_ENV_MAP -o ${GL_DIR}/rsp_gl_pipeline_env.S
node ${RSPL} ${GL_PIPELINE_RSPL_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -D ENABLE_NORMALIZE -o ${GL_DIR}/rsp_gl_pipeline_nrm.S
node ${RSPL} ${GL_PIPELINE_RSPL_RSPL} --magma --reorder --opt-time=60 --opt-worker=16 -D ENABLE_ENV_MAP -D ENABLE_NORMALIZE -o ${GL_DIR}/rsp_gl_pipeline_env_nrm.S
