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
    rm -rf /tmp/nrpe.tar.gz /tmp/nrpe-4.1.3
RUN mkdir -p /test
COPY *.crt *.key /test/
