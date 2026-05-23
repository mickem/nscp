FROM alpine:3.19

# netcat for raw TCP fetch, grep/sh for the assertion harness
RUN apk add --no-cache netcat-openbsd grep coreutils

# Tiny wrapper so `docker run check_mk_client <host> <port>` dumps the agent
# text to stdout. The check_mk agent protocol is plain TCP: the server pushes
# the dump and closes, so a 5-second read timeout is more than enough.
RUN printf '#!/bin/sh\nexec nc -w 5 "$1" "$2"\n' > /usr/local/bin/fetch-mk \
    && chmod +x /usr/local/bin/fetch-mk

ENTRYPOINT ["/usr/local/bin/fetch-mk"]
