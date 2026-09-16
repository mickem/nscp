# Integration-test fixture: a bare gearmand job server.
#
# Tier 2 of the GearmanClient test plan: the test itself plays the
# monitoring core (submits jobs, reads results) through the TypeScript
# gearman fixture in tests/src/gearman.ts, so no Naemon or Nagios is
# involved. Debian's gearman-job-server package starts in well under a
# second. gearman-tools adds the `gearman` / `gearadmin` CLIs, handy for
# poking at queues from `docker exec`.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        gearman-job-server \
        gearman-tools \
    && rm -rf /var/lib/apt/lists/*

EXPOSE 4730

# Foreground, logging to stderr so `docker logs` shows connections and
# errors. gearmand keeps its queues in memory; a container restart empties
# them, which is exactly what the reconnect test wants.
CMD ["gearmand", "--listen=0.0.0.0", "--port=4730", "--verbose=INFO", "--log-file=stderr"]
