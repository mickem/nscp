# Integration-test fixture: Naemon + ConSol Mod-Gearman + gearmand in one
# container.
#
# ConSol Labs publishes Debian packages for Naemon and Mod-Gearman, so this
# is a package install only: naemon-core, the NEB module
# (mod-gearman-module, /usr/lib/mod_gearman/mod_gearman_naemon.o) and the
# tools (mod-gearman-tools: send_gearman, gearman_top, check_gearman).
# Should the repository move again (it is migrating to OBS home:naemon),
# the fallback is to build naemon-core and sni/mod_gearman from their
# GitHub tags the way nagios-gearman.Dockerfile builds the Nagios pair; the
# entrypoint is the same either way.
#
# gearmand runs in the same container so the NEB module talks to localhost;
# port 4730 is exposed for the worker under test (NSClient++ on the host).
# The entrypoint writes the whole core configuration itself, see
# Dockerfiles/entrypoints/gearman-core.sh, and with CAPTURE_DIR set dumps
# raw job/result payloads instead of serving checks (used by
# modules/GearmanClient/fixtures/capture.sh).
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl gnupg \
    && curl -fsSL "https://labs.consol.de/repo/stable/RPM-GPG-KEY" \
        | gpg --dearmor -o /etc/apt/trusted.gpg.d/labs-consol-stable.gpg \
    && echo "deb https://labs.consol.de/repo/stable/debian bookworm main" \
        > /etc/apt/sources.list.d/labs-consol-stable.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        naemon-core \
        mod-gearman-module \
        mod-gearman-tools \
        gearman-job-server \
        gearman-tools \
        openssl \
    && rm -rf /var/lib/apt/lists/*

ENV GEARMAN_CORE=naemon
ENV CORE_BIN=/usr/bin/naemon
ENV NEB_MODULE=/usr/lib/mod_gearman/mod_gearman_naemon.o
ENV SEND_GEARMAN=/usr/bin/send_gearman
ENV GEARMAN_TOP=/usr/bin/gearman_top
# Shared secret and routing; tests override via -e.
ENV GEARMAN_KEY=nscp-test-key
ENV GEARMAN_ENCRYPTION=yes
ENV GEARMAN_HOSTGROUP=gearman-test

EXPOSE 4730

COPY Dockerfiles/entrypoints/gearman-core.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
