#!/bin/sh
# TLS-terminating Graphite/carbon receiver for graphite-tls.test.ts.
#
# Carbon's own line receiver is plaintext-only, so production "Graphite over
# TLS" means a TLS-terminating proxy (stunnel / nginx / carbon-relay-ng) in
# front of carbon. socat plays that proxy here: it accepts TLS on 2003
# (presenting the bind-mounted server cert), decrypts, and appends the
# plaintext carbon lines to /data/metrics.txt, which the test reads back via
# `docker exec`. verify=0 => one-way TLS (no client certificate required).
set -eu

CERT="${CERT:-/certs/server.crt}"
KEY="${KEY:-/certs/server.key}"
OUT="${OUT:-/data/metrics.txt}"

touch "$OUT"

exec socat -u \
    "OPENSSL-LISTEN:2003,reuseaddr,fork,cert=${CERT},key=${KEY},verify=0" \
    "OPEN:${OUT},creat,append"
