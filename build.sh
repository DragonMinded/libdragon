#!/bin/bash
make -j8 clean && \
make -j8 && \
make install && \
make -j8 tools && \
make tools-install && \
git clone https://github.com/networkfusion/libmikmod.git /tmp/libmikmod && \
pushd /tmp/libmikmod/n64 && \
git checkout 738b1e8b11b470360b1b919680d1d88429d9d174 && \
make -j8 && \
make install && \
popd && \
rm -rf /tmp/libmikmod && \
make -j8 examples