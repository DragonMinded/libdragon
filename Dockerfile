FROM ubuntu:16.04

RUN apt-get update && apt-get install -yq wget bzip2 gcc g++ make file libmpfr-dev libmpc-dev libpng-dev zlib1g-dev texinfo git && apt-get clean

ENV N64_INST=/usr/local

COPY ./tools/build /tmp/tools/build
WORKDIR /tmp/tools
RUN JOBS=8 ./build && rm -rf /tmp/tools

COPY . /libdragon
WORKDIR /libdragon

RUN make --jobs 8 && make install && make --jobs 8 tools && make tools-install && rm -rf /libdragon/* && git clone https://github.com/networkfusion/libmikmod.git /tmp/libmikmod && cd /tmp/libmikmod/n64 && make --jobs 8 && make install && rm -rf /tmp/libmikmod