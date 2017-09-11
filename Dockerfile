FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -yq wget bzip2 gcc g++ make file libmpfr-dev libmpc-dev libpng-dev zlib1g-dev texinfo

ENV N64_INST=/usr/local

COPY ./tools/build /tmp/tools/build
RUN JOBS=8 /tmp/tools/build && rm -rf /tmp/tools

COPY . /libdragon
WORKDIR /libdragon

RUN make --jobs 8 && make install
RUN make --jobs 8 tools && make tools-install