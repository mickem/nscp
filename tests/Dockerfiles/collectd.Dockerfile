# A real collectd daemon used by collectd-daemon.test.ts to prove NSClient++'s
# CollectdClient speaks the binary network protocol well enough for an actual
# collectd to parse it. The `network` plugin listens on UDP 25826 and the `csv`
# plugin writes every received value-list to /data/<host>/<plugin>/<type>... —
# the test reads those files back via `docker exec` to confirm collectd
# accepted (parsed + type-checked) our packets.
#
# Unlike the other receivers this one is published with `-p ...:25826/udp` via
# raw `docker run` (testcontainers only maps TCP), so there is no wait strategy
# baked in here; the test waits for the "entering read-loop" log line.
# ubuntu:24.04 (collectd 5.12, built with libgcrypt so the Sign/Encrypt
# listeners work) rather than debian: functionally equivalent, and Ubuntu
# mirrors are reachable in more restricted build environments.
FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends collectd-core \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /data

COPY Dockerfiles/entrypoints/collectd.conf /etc/collectd/collectd.conf
COPY Dockerfiles/entrypoints/collectd.auth /etc/collectd/collectd.auth

# 25826 = SecurityLevel None, 25827 = Sign, 25828 = Encrypt (see collectd.conf).
EXPOSE 25826/udp 25827/udp 25828/udp

# -f: run in the foreground (so logs stream and the container stays attached).
# -C: use only our config, not the distro's default plugin soup.
ENTRYPOINT ["collectd", "-f", "-C", "/etc/collectd/collectd.conf"]
