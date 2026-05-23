FROM alpine:3.14

RUN apk add --no-cache \
    build-base \
    cmake \
    openssl-dev \
    tar

RUN wget https://github.com/NagiosEnterprises/nrpe/releases/download/nrpe-4.1.3/nrpe-4.1.3.tar.gz -O /tmp/nrpe.tar.gz && \
    tar -xzf /tmp/nrpe.tar.gz -C /tmp && \
    cd /tmp/nrpe-4.1.3 && \
    ./configure --enable-ssl && \
    make all && \
    cp src/check_nrpe /usr/bin/check_nrpe && \
    sed -i '/#define.*MAX_PACKETBUFFER_LENGTH/c\#define MAX_PACKETBUFFER_LENGTH 4096' include/common.h && \
    make all && \
    cp src/check_nrpe /usr/bin/check_nrpe_4096 && \
    rm -rf /tmp/nrpe.tar.gz /tmp/nrpe-4.1.3
RUN mkdir -p /test
# Certs are mounted at runtime by the .ts test (see nrpe-tls.test.ts —
# generateCertChain writes into nscp.scratch("nrpe_test") and the certs are
# bind-mounted into /test on `docker run`). No build-time cert copy needed.
