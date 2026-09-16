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

# ConSol rotated the repository signing key in 2026: the old RPM-GPG-KEY no
# longer signs the Debian dists, so an image built with it dies on
# "NO_PUBKEY CBB9B38BE1B9D330". The armoured 2026 key below is the one the
# repository's own install instructions now hand out, scoped to this list
# file with signed-by rather than trusted for every repository in the image.
ARG CONSOL_KEY=monitoring-repo-consol-de-gpg-2026.asc

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl \
    && curl -fsSL "https://labs.consol.de/repo/stable/${CONSOL_KEY}" \
        -o "/etc/apt/trusted.gpg.d/${CONSOL_KEY}" \
    && echo "deb [signed-by=/etc/apt/trusted.gpg.d/${CONSOL_KEY}]" \
        "https://labs.consol.de/repo/stable/debian bookworm main" \
        > /etc/apt/sources.list.d/labs-consol-stable.list \
    && apt-get update && apt-get install -y --no-install-recommends \
        naemon-core \
        mod-gearman-module \
        mod-gearman-tools \
        gearman-job-server \
        gearman-tools \
        openssl \
    && rm -rf /var/lib/apt/lists/*

# Mod-Gearman 5.2 moved both files this fixture names: the NEB module into
# the multiarch libdir (/usr/lib/<triplet>/mod_gearman/) and send_gearman
# into the plugin dir. Link them to fixed paths rather than spelling the
# triplet, which differs between an amd64 and an arm64 build of this image;
# dpkg -L is the package's own answer to "where did it put them".
RUN mkdir -p /opt/mod_gearman \
    && ln -s "$(dpkg -L mod-gearman-module | grep -m1 'mod_gearman_naemon\.o$')" \
        /opt/mod_gearman/mod_gearman_naemon.o \
    && ln -s "$(dpkg -L mod-gearman-tools | grep -m1 '/send_gearman$')" \
        /opt/mod_gearman/send_gearman

ENV GEARMAN_CORE=naemon
ENV CORE_BIN=/usr/bin/naemon
ENV NEB_MODULE=/opt/mod_gearman/mod_gearman_naemon.o
ENV SEND_GEARMAN=/opt/mod_gearman/send_gearman
ENV GEARMAN_TOP=/usr/bin/gearman_top
# Shared secret and routing; tests override via -e.
ENV GEARMAN_KEY=nscp-test-key
ENV GEARMAN_ENCRYPTION=yes
ENV GEARMAN_HOSTGROUP=gearman-test

EXPOSE 4730

COPY Dockerfiles/entrypoints/gearman-core.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
