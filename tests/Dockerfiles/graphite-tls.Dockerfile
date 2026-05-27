# TLS-terminating Graphite/carbon receiver used by graphite-tls.test.ts.
# socat terminates TLS on 2003 (using a bind-mounted server cert/key) and
# appends the decrypted carbon lines to /data/metrics.txt (read back via
# `docker exec`). This stands in for the stunnel/nginx TLS proxy that fronts
# carbon in production, since carbon's own line receiver is plaintext-only.
FROM debian:stable-slim

RUN apt-get update && \
    apt-get install -y socat && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /data /certs

COPY Dockerfiles/entrypoints/graphite-tls.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 2003

ENTRYPOINT ["/entrypoint.sh"]
