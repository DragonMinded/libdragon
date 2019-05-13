FROM anacierdem/libdragon:base
COPY . /libdragon
WORKDIR /libdragon
ARG LIBDRAGON_VERSION_MAJOR
ARG LIBDRAGON_VERSION_MINOR
ARG LIBDRAGON_VERSION_REVISION
RUN make --jobs 8 clean && make --jobs 8 && make install && make --jobs 8 tools && make tools-install && rm -rf /libdragon/*