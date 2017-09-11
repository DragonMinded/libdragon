FROM vieux/libdragon:latest

RUN apt-get update
RUN apt-get install -yq git

ENV N64_INST=/usr/local

RUN git clone https://github.com/networkfusion/libmikmod.git /tmp/libmikmod

COPY . /examples
WORKDIR /examples

RUN cd /tmp/libmikmod/n64  && git apply /examples/dfsdemo/libmikmod.patch && make --jobs 8 && make install

RUN make --jobs 8