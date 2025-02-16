# syntax=docker/dockerfile:1
# V0 - Use this comment to force a re-build without changing the contents

ARG BASE_IMAGE=ubuntu:22.04

# Stage 1 - Build the toolchain
FROM ${BASE_IMAGE} AS builder

# Setup paths for the libdragon toolchain
ARG N64_INST=/n64_toolchain
ARG BUILD_PATH=/tmp/build
ARG DOWNLOAD_PATH=/tmp/download
ENV N64_INST=${N64_INST}
ENV BUILD_PATH=${BUILD_PATH}
ENV DOWNLOAD_PATH=${DOWNLOAD_PATH}

# Install system dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache \
    && apt update \
    && apt upgrade -y \
    && apt install -y --no-install-recommends \
        bzip2 \
        ca-certificates \
        file \
        g++ \
        gcc \
        git \
        libmpc-dev \
        libmpfr-dev \
        make \
        texinfo \
        wget \
        zlib1g-dev \
    && apt autoremove -yq

# Copy the build scripts into the container
COPY ./tools/build-toolchain.sh ./tools/build-gdb.sh /tools/

# Run the build scripts and cleanup unnecessary files
RUN --mount=target=${DOWNLOAD_PATH},type=cache,sharing=locked \
    /tools/build-toolchain.sh && \
    /tools/build-gdb.sh && \
    rm -rf ${N64_INST}/share/locale/*

# Stage 2 - Prepare minimal image
FROM ${BASE_IMAGE} AS runner

# Setup paths for the libdragon toolchain
ARG N64_INST=/n64_toolchain
ENV N64_INST=${N64_INST}
ENV PATH="${N64_INST}/bin:$PATH"

# Install system dependencies
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
    --mount=target=/var/cache/apt,type=cache,sharing=locked \
    rm -f /etc/apt/apt.conf.d/docker-clean \
    && echo 'Binary::apt::APT::Keep-Downloaded-Packages "true";' > /etc/apt/apt.conf.d/keep-cache \
    && apt update \
    && apt upgrade -y \
    && apt install -y --no-install-recommends \
        g++ \
        gcc \
        git \
        make \
        xxd \
    && apt autoremove -yq

# Copy over built toolchain from previous stage
COPY --from=builder ${N64_INST} ${N64_INST}
