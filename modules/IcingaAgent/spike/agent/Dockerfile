# Runtime for an NSClient++ build tree (Ubuntu 24.04 build) mounted at /nscp.
ARG BASE=ubuntu:24.04
FROM ${BASE}
RUN apt-get update && apt-get install -y --no-install-recommends \
        libboost-chrono1.83.0t64 libboost-filesystem1.83.0 libboost-json1.83.0 \
        libboost-program-options1.83.0 libboost-thread1.83.0 libprotobuf32t64 \
        libssl3t64 zlib1g ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY nsclient.ini /etc/nscp/nsclient.ini
COPY entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
