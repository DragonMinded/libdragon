# syntax=docker/dockerfile:1
# V0 - Use this comment to force a re-build without changing the contents

# Stage 1 - Build the toolchain
FROM ubuntu:22.04
ARG N64_INST=/n64_toolchain
ENV N64_INST=${N64_INST}

# install dependencies
RUN apt-get update
RUN apt-get install -yq wget bzip2 gcc g++ make file libmpfr-dev libmpc-dev zlib1g-dev texinfo git gcc-multilib

# Build
COPY ./tools/build-toolchain.sh /tmp/tools/build-toolchain.sh
WORKDIR /tmp/tools
RUN ./build-toolchain.sh

# Remove locale strings which are not so important in our use case
RUN rm -rf ${N64_INST}/share/locale/*

# Stage 2 - Prepare minimal image
FROM ubuntu:22.04
ARG N64_INST=/n64_toolchain
ENV N64_INST=${N64_INST}
ENV PATH="${N64_INST}/bin:$PATH"

COPY --from=0 ${N64_INST} ${N64_INST}
RUN apt-get update && \
    apt-get install -yq gcc g++ make git  && \
    apt-get clean && \
    apt autoremove -yq
