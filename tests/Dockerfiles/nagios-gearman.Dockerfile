# Integration-test fixture: Nagios Core 4.5 + Nagios-Mod-Gearman + gearmand
# in one container.
#
# Nagios Enterprises forked ConSol's Mod-Gearman in 2025 as
# nagios-mod-gearman for Nagios Core 4.5 and later. Debian and Ubuntu still
# ship Nagios 4.4.x, which the fork does not support, and the fork has no
# apt repository, so both are built from their GitHub release tarballs in a
# builder stage. The runtime stage carries only the built trees plus the
# shared libraries they link.
#
# gearmand runs in the same container so the NEB module talks to localhost;
# port 4730 is exposed for the worker under test (NSClient++ on the host).
# The entrypoint writes the whole core configuration itself, see
# Dockerfiles/entrypoints/gearman-core.sh, and with CAPTURE_DIR set dumps
# raw job/result payloads instead of serving checks (used by
# modules/GearmanClient/fixtures/capture.sh).
ARG NAGIOS_VERSION=4.5.14
ARG NMG_VERSION=1.0.1

FROM debian:bookworm-slim AS builder
ARG NAGIOS_VERSION
ARG NMG_VERSION

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl \
        build-essential autoconf automake libtool pkg-config \
        libgearman-dev libssl-dev libltdl-dev libncurses-dev \
    && rm -rf /var/lib/apt/lists/*

# `make install-base` chowns to the nagios user, so it has to exist here too.
RUN groupadd -r nagios && useradd -r -g nagios -d /usr/local/nagios -s /usr/sbin/nologin nagios

WORKDIR /src
RUN curl -fsSL "https://github.com/NagiosEnterprises/nagioscore/archive/refs/tags/nagios-${NAGIOS_VERSION}.tar.gz" \
        | tar xz \
    && cd "nagioscore-nagios-${NAGIOS_VERSION}" \
    && ./configure --prefix=/usr/local/nagios \
        --with-nagios-user=nagios --with-nagios-group=nagios \
        --with-command-user=nagios --with-command-group=nagios \
    && make -j"$(nproc)" nagios \
    && make install-base install-commandmode

RUN curl -fsSL "https://github.com/NagiosEnterprises/nagios-mod-gearman/archive/refs/tags/v${NMG_VERSION}.tar.gz" \
        | tar xz \
    && cd "nagios-mod-gearman-${NMG_VERSION}" \
    && ./autogen.sh \
    && ./configure --prefix=/usr/local/nagios-mod-gearman --with-user=nagios \
    && make -j"$(nproc)" \
    && make install

FROM debian:bookworm-slim

# Runtime: gearmand plus the `gearman`/`gearadmin` CLIs (capture mode and
# docker-exec diagnostics), the libraries the built binaries link, and the
# openssl CLI (capture mode decrypts payloads with it). setpriv comes with
# util-linux, which the base image already has.
RUN apt-get update && apt-get install -y --no-install-recommends \
        gearman-job-server \
        gearman-tools \
        libgearman8 \
        libssl3 \
        libltdl7 \
        libncurses6 \
        libtinfo6 \
        libuuid1 \
        openssl \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -r nagios && useradd -r -g nagios -d /usr/local/nagios -s /usr/sbin/nologin nagios

COPY --from=builder /usr/local/nagios /usr/local/nagios
COPY --from=builder /usr/local/nagios-mod-gearman /usr/local/nagios-mod-gearman
RUN chown -R nagios:nagios /usr/local/nagios /usr/local/nagios-mod-gearman

ENV GEARMAN_CORE=nagios
ENV CORE_BIN=/usr/local/nagios/bin/nagios
ENV NEB_MODULE=/usr/local/nagios-mod-gearman/lib/nagios-mod-gearman/nagios-mod-gearman.o
ENV SEND_GEARMAN=/usr/local/nagios-mod-gearman/bin/nagios-send-gearman
ENV GEARMAN_TOP=/usr/local/nagios-mod-gearman/bin/nagios-gearman-top
# Shared secret and routing; tests override via -e.
ENV GEARMAN_KEY=nscp-test-key
ENV GEARMAN_ENCRYPTION=yes
ENV GEARMAN_HOSTGROUP=gearman-test

EXPOSE 4730

COPY Dockerfiles/entrypoints/gearman-core.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
