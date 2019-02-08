FROM anacierdem/libdragon:base
COPY . /libdragon
WORKDIR /libdragon
RUN make --jobs 8 && make install && make --jobs 8 tools && make tools-install && rm -rf /libdragon/*