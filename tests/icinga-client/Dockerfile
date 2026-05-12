# Builds Icinga2's check_nscp_api plugin from upstream source so we can run it
# against this build's WEBServer in run-test.bat. Same pattern as tests/nrpe:
# multi-stage build, runtime image only carries the binary + its runtime
# dependencies.
#
# check_nscp_api links against Icinga2's `base` library (Application, Url,
# JSON helpers, …), so we can't just compile the .cpp standalone — we have
# to invoke Icinga2's CMake target. Building only that target avoids the
# full Icinga2 build (otherwise this would take 20+ minutes).
#
# The version is pinned via ARG so the test is reproducible. Bump it when a
# new Icinga2 release adds a relevant behaviour change to check_nscp_api.

ARG ICINGA2_VERSION=v2.14.6

FROM debian:bookworm-slim AS builder
ARG ICINGA2_VERSION

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    cmake \
    g++ \
    make \
    pkg-config \
    libboost-all-dev \
    libssl-dev \
    libyajl-dev \
    flex \
    bison \
    && rm -rf /var/lib/apt/lists/*

# Shallow clone — only the tagged source, no history. The build itself only
# needs the plugins/ and lib/base subtrees but CMake's configure step needs
# the whole tree to wire everything up.
RUN git clone --depth=1 --branch "${ICINGA2_VERSION}" https://github.com/Icinga/icinga2 /src/icinga2

WORKDIR /src/icinga2

# Configure with the heavy bits turned off — we only need the `base` library
# and the check_nscp_api plugin target. `USE_SYSTEMD=OFF` keeps libsystemd
# out of the runtime image; we don't need it for a CLI binary.
RUN cmake -DCMAKE_BUILD_TYPE=Release \
          -DICINGA2_WITH_TESTS=OFF \
          -DICINGA2_WITH_LIVESTATUS=OFF \
          -DICINGA2_WITH_NOTIFICATION=OFF \
          -DICINGA2_WITH_PERFDATA=OFF \
          -DICINGA2_WITH_COMPAT=OFF \
          -DICINGA2_WITH_CHECKER=OFF \
          -DICINGA2_WITH_MYSQL=OFF \
          -DICINGA2_WITH_PGSQL=OFF \
          -DUSE_SYSTEMD=OFF \
          -B build . && \
    cmake --build build --target check_nscp_api -j"$(nproc)"

# -----------------------------------------------------------------------------

FROM debian:bookworm-slim

# Runtime deps mirror the libs check_nscp_api was linked against. boost
# versions are pinned to bookworm's apt-cache; bump alongside FROM.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libboost-program-options1.74.0 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-context1.74.0 \
    libboost-coroutine1.74.0 \
    libssl3 \
    libyajl2 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/icinga2/build/plugins/check_nscp_api /usr/local/bin/check_nscp_api

# Smoke-test that the binary at least runs (it should print --help when
# invoked with -h and exit 3 on missing args). Catches build failures that
# silently produce an unusable binary.
RUN /usr/local/bin/check_nscp_api --help >/dev/null

ENTRYPOINT ["/usr/local/bin/check_nscp_api"]
