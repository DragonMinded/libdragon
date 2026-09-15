#!/usr/bin/env bash
#
# This script is used to push the ARM64 Docker image of the toolchain to the GitHub
# Container Registry. It is required until we manage to change the CI to automatically
# build the image for ARM64 as well. 


set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

# === Configuration ===
OWNER="dragonminded"
IMAGE="libdragon"
REGISTRY="ghcr.io/${OWNER}/${IMAGE}"
TAG="latest"

# === Utility functions ===

error() {
    echo "❌ $*" >&2
    exit 1
}

info() {
    echo "👉 $*"
}

# === Pre-flight checks ===

# 1. Check that Docker is running
docker info >/dev/null 2>&1 || error "Docker is not running. Please start Docker first."

# 2. Check that Buildx is available
if ! docker buildx version >/dev/null 2>&1; then
    error "Docker Buildx is not available. Install a recent version of Docker Desktop or run: brew install docker-buildx"
fi

# 3. Check authentication to ghcr.io
info "Checking authentication to ghcr.io..."
if ! docker system info | grep -q "ghcr.io"; then
    info "No active login found for ghcr.io, verifying access..."
fi

# 4. Verify actual access (test pull)
if ! docker pull "${REGISTRY}:${TAG}" >/dev/null 2>&1; then
    info "Unable to pull ${REGISTRY}:${TAG} — verifying login..."
    if ! docker login ghcr.io >/dev/null 2>&1; then
        error "You are not authenticated to ghcr.io. Please log in with: echo \$CR_PAT | docker login ghcr.io -u <github_user> --password-stdin"
    fi
    info "Login successful, retrying test pull..."
    docker pull "${REGISTRY}:${TAG}" >/dev/null 2>&1 || info "Test pull failed (this may be normal if the image doesn't exist yet)."
fi

info "✅ Docker authentication to ghcr.io verified."

# === Build ARM64 image ===
info "Building ARM64 image from local Dockerfile..."
docker buildx create --use --name multiarch-builder >/dev/null 2>&1 || docker buildx use multiarch-builder
docker buildx inspect --bootstrap >/dev/null

docker buildx build \
    --platform linux/arm64 \
    -t "${REGISTRY}:${TAG}-arm64" \
    -f Dockerfile \
    --load .

# === Fetch existing x86_64 image ===
info "Pulling existing x86_64 image..."
docker pull "${REGISTRY}:${TAG}-amd64" || docker pull "${REGISTRY}:${TAG}" || {
    error "Unable to find existing x86 image on GHCR (required to create multi-arch manifest)."
}

# === Create and push multi-arch manifest ===
info "Creating and pushing multi-architecture manifest..."
docker manifest create "${REGISTRY}:${TAG}" \
    --amend "${REGISTRY}:${TAG}-amd64" \
    --amend "${REGISTRY}:${TAG}-arm64"

docker manifest push "${REGISTRY}:${TAG}"

info "🎉 Done! Multi-platform image pushed to ${REGISTRY}:${TAG}"
