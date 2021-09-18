# syntax=docker/dockerfile:1

ARG N64_INST=/n64_toolchain

# Stage 1 - Build the toolchain
FROM ubuntu:18.04

# install dependencies
RUN apt-get update
RUN apt-get install -yq wget bzip2 gcc g++ make file libmpfr-dev libmpc-dev zlib1g-dev texinfo git gcc-multilib
RUN apt-get clean

# Build
COPY ./tools/* /tmp/tools
WORKDIR /tmp/tools
RUN ./build-toolchain.sh

# Strip executables
RUN find ${N64_INST}/bin -type f | xargs strip
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/plugin/gengtype
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/liblto_plugin.so.0.0.0
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/liblto_plugin.so.0
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/lto-wrapper
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/collect2
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/cc1plus
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/cc1
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/install-tools/fixincl
RUN strip ${N64_INST}/libexec/gcc/mips64-elf/10.2.0/lto1
RUN rm -rf ${N64_INST}/share/locale/*

# Stage 2 - Prepare minimal image
FROM ubuntu:18.04
COPY --from=0 ${N64_INST} ${N64_INST}
RUN apt-get update && \
    apt-get install -yq gcc g++ make libpng-dev git  && \
    apt-get clean  && \
    apt autoremove -yq