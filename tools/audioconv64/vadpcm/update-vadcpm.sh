#!/usr/bin/env bash
set -euo pipefail

PREFIX="tools/audioconv64/vadpcm"
REMOTE_URL="https://github.com/depp/vadpcm.git"
BRANCH="main"

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

git fetch "$REMOTE_URL" "$BRANCH"
split_commit="$(git subtree split --prefix=codec FETCH_HEAD)"
git subtree pull --prefix "$PREFIX/codec" . "$split_commit" --squash
echo "Update applied."
