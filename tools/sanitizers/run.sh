#!/usr/bin/env bash
# Configure, build, and run unit tests under sanitizers.
#
# Usage:
#   tools/sanitizers/run.sh                  # ASan + UBSan in build-asan/
#   tools/sanitizers/run.sh -R parsers_where # forwards args to ctest
#   SANITIZE=thread tools/sanitizers/run.sh  # TSan instead (separate dir)
#   BUILD_DIR=tmp tools/sanitizers/run.sh    # custom build dir
#   CMAKE_ARGS="-DNSCP_WEB_BACKEND=beast -DBUILD_MODULE_WEBServer=OFF" \
#       tools/sanitizers/run.sh              # extra -D flags forwarded to cmake
#
# Sanitizer runtime options (ASAN_OPTIONS / UBSAN_OPTIONS / LSAN_OPTIONS) are
# baked into the per-test ENVIRONMENT property by CMake when NSCP_SANITIZE is
# set, so plain `ctest --test-dir <BUILD_DIR>` works after the first build.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

SANITIZE="${SANITIZE:-address,undefined}"
BUILD_DIR="${BUILD_DIR:-build-${SANITIZE//,/+}}"
JOBS="${JOBS:-$(nproc)}"

echo "==> Configuring $BUILD_DIR with NSCP_SANITIZE=$SANITIZE"
# Extra cmake -D flags (e.g. picking a web backend or disabling a module whose
# deps aren't installed) are word-split from CMAKE_ARGS; empty by default.
# shellcheck disable=SC2206
EXTRA_CMAKE_ARGS=( ${CMAKE_ARGS:-} )
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DNSCP_SANITIZE="$SANITIZE" \
    "${EXTRA_CMAKE_ARGS[@]}"

echo "==> Building (-j$JOBS)"
# detect_leaks=0 only during the build: docs / proto codegen helpers leak
# their Python arenas at exit but that's not actionable. Test-time options
# are set per-test via CMake.
ASAN_OPTIONS=detect_leaks=0 cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> Running unit tests under $SANITIZE"
ctest --test-dir "$BUILD_DIR" -R '_test$' --output-on-failure "$@"
