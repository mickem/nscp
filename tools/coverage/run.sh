#!/usr/bin/env bash
# Configure, build, and run the test suites with gcov instrumentation, then
# render lcov-style HTML + Cobertura reports.
#
# Usage:
#   tools/coverage/run.sh                     # unit + integration → coverage/
#   SUITES=unit tools/coverage/run.sh         # C++ unit tests only
#   SUITES=integration tools/coverage/run.sh  # tests/ jest suite only
#   BUILD_DIR=tmp/cov tools/coverage/run.sh   # custom build dir
#   SKIP_BUILD=1 tools/coverage/run.sh        # re-run tests in an existing tree
#   JEST_ARGS="checkdisk" tools/coverage/run.sh          # jest test pattern
#   CTEST_ARGS="-R parsers_where_test" tools/coverage/run.sh
#   CMAKE_ARGS="-DBUILD_MODULE_WEBServer=OFF" tools/coverage/run.sh
#
# Output (under coverage/):
#   html/index.html    merged report, click through to per-file annotated source
#   unit.html          C++ unit tests (ctest) alone, one-page summary
#   integration.html   tests/ jest suite alone, one-page summary
#   cobertura.xml      merged, for Codecov / CI coverage gates
#
# Only the merged report gets per-file detail pages - a full --html-details set
# is ~80MB, and three of them is not something to hand a CI artifact store. For
# annotated source of one suite alone, run just that suite: the merged report
# then contains only its data.
#
# Requires gcovr (apt-get install gcovr / pip install gcovr). The gcov used
# must match the compiler that built the tree — override with GCOV=gcov-13,
# or GCOV="llvm-cov gcov" for a clang build.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
ROOT="$(pwd)"

BUILD_DIR="${BUILD_DIR:-build-coverage}"
OUT_DIR="${OUT_DIR:-coverage}"
SUITES="${SUITES:-all}"
JOBS="${JOBS:-$(nproc)}"
GCOV="${GCOV:-gcov}"

# Exit status of the test suites, reported at the very end. A failing suite
# must not abort the run: a report is most useful precisely when something
# broke, and losing it to `set -e` means re-running the whole build.
test_status=0

# Absolute path to the build tree, so the jest harness (which runs from
# tests/) can be handed a usable NSCP_BIN regardless of how BUILD_DIR was
# spelled on the command line.
case "$BUILD_DIR" in
    /*) BUILD_ABS="$BUILD_DIR" ;;
    *) BUILD_ABS="$ROOT/$BUILD_DIR" ;;
esac

run_unit=0
run_integration=0
case "$SUITES" in
    all) run_unit=1; run_integration=1 ;;
    unit) run_unit=1 ;;
    integration) run_integration=1 ;;
    *) echo "SUITES must be one of: all, unit, integration" >&2; exit 2 ;;
esac

command -v gcovr > /dev/null || {
    echo "gcovr not found - install it with 'apt-get install gcovr'" >&2
    exit 1
}

# Stale .gcda are not merely useless, they are poison: gcov refuses them with
# "stamp mismatch with notes file" as soon as the sources have moved on, and a
# half-refused data set silently under-reports. Wipe before every run.
wipe_gcda() {
    find "$BUILD_DIR" -name '*.gcda' -delete
}

if [ -z "${SKIP_BUILD:-}" ]; then
    echo "==> Configuring $BUILD_DIR with NSCP_COVERAGE=ON"
    # shellcheck disable=SC2206
    EXTRA_CMAKE_ARGS=( ${CMAKE_ARGS:-} )
    # CauseCrashes exists only to crash the daemon on demand. Nothing exercises
    # it here, so all it contributes is a permanently 0% file dragging the
    # totals down - and were it ever invoked, the crash would take the
    # unflushed gcov counters with it. Listed before CMAKE_ARGS so a caller can
    # still turn it back on with CMAKE_ARGS="-DBUILD_MODULE_CauseCrashes=ON".
    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DNSCP_COVERAGE=ON \
        -DBUILD_MODULE_CauseCrashes=OFF \
        "${EXTRA_CMAKE_ARGS[@]}"

    echo "==> Building (-j$JOBS)"
    cmake --build "$BUILD_DIR" -j"$JOBS"
fi

mkdir -p "$OUT_DIR"

# Tracefiles left by an earlier run describe an earlier tree. The merge below
# picks up whatever it finds in $OUT_DIR, so a stale one is not merely out of
# date - gcovr aborts on an md5 mismatch the moment a covered source file has
# changed since. Only the suites that run here belong in the merge (see the
# one-suite note above), so drop the leftovers before starting.
rm -f "$OUT_DIR/unit.json" "$OUT_DIR/integration.json"

# gcovr filters shared by every report. --root is the repo, so only our own
# sources are considered; the excludes drop the test bodies themselves,
# generated protobuf code and the FetchContent'ed googletest under _deps.
#
# module.cpp/module.hpp are the per-module dispatch/export glue that CMake
# generates into the build tree from module.json (66 files, ~4600 lines, none
# of them hand-written). Nobody can act on a gap in generated code by writing
# a test for it, and leaving it in costs ~2.6 points of line coverage. But it
# does carry one signal worth keeping in mind: a module.cpp at 0% means no
# test ever *loaded* that module, even if its own sources are well covered by
# a unit test that links them directly. To see that view:
#
#   gcovr --add-tracefile coverage/unit.json \
#         --filter '.*/module\.cpp$' --txt glue.txt --print-summary
gcovr_common=(
    --root "$ROOT"
    --gcov-executable "$GCOV"
    "$BUILD_ABS"
    --exclude '.*_test\.cpp'
    --exclude '.*\.pb\.(cc|h)$'
    --exclude '.*/_deps/.*'
    --exclude '^ext/.*'
    --exclude-unreachable-branches
    --exclude-throw-branches
    --gcov-ignore-parse-errors
)

# gcov names its output after the source file and writes it into gcovr's
# --root (not the current directory), so two objects that include the same
# header produce the same path there. gcovr serialises its own workers on that
# directory, but the guard is an in-process lock: a second gcovr over the same
# tree knows nothing about the first, and they delete each other's files. The
# loud failure is a FileNotFoundError part way through; the quiet one is worse,
# because --gcov-ignore-parse-errors turns a lost file into missing coverage in
# a report that still looks complete.
#
# So: hold a lock for the whole run, and read the gcov data serially. Since
# every .gcda resolves against --root first, gcovr's own workers queue up on
# that one directory anyway - -j buys nothing here and costs memory. The build
# and the test runs above still use it.
exec 9> "$OUT_DIR/.gcovr.lock"
flock 9

# A run that dies part way leaves those temporary .gcov files behind in the
# repo (the crash that prompted the lock above left ~100 of them). gcovr
# removes them as it goes when it finishes normally; sweep whatever is left.
cleanup_gcov() { find "$ROOT" -maxdepth 1 -name '*.gcov' -delete 2> /dev/null || true; }
trap cleanup_gcov EXIT

# Applied when *rendering*, not when reading gcov, so the .json tracefiles
# keep the generated glue and the one-liner above stays a cheap tracefile
# query rather than a full re-read of the build tree.
gcovr_report_excludes=(
    --exclude '.*/module\.(cpp|hpp)$'
)

if [ "$run_unit" = 1 ]; then
    echo "==> Running unit tests (ctest)"
    wipe_gcda
    # shellcheck disable=SC2086
    ctest --test-dir "$BUILD_DIR" -R '_test$' --output-on-failure ${CTEST_ARGS:-} \
        || { test_status=1; echo "!!! unit tests failed - collecting coverage anyway"; }
    echo "==> Collecting unit coverage"
    gcovr "${gcovr_common[@]}" --json "$ROOT/$OUT_DIR/unit.json"
    gcovr --root "$ROOT" --add-tracefile "$ROOT/$OUT_DIR/unit.json" \
        "${gcovr_report_excludes[@]}" \
        --html "$ROOT/$OUT_DIR/unit.html" \
        --print-summary
fi

if [ "$run_integration" = 1 ]; then
    echo "==> Running integration tests (jest)"
    wipe_gcda
    if [ ! -d tests/node_modules ]; then
        ( cd tests && npm ci )
    fi
    # The jest harness spawns $NSCP_BIN and lets the modules dlopen from the
    # same tree, so every .so is instrumented too. It stops the daemon with
    # SIGTERM (tests/src/nscp.ts), which CommandClient turns into a graceful
    # shutdown - that is what lets gcov flush its counters at exit. A SIGKILL
    # shutdown would silently produce no coverage at all.
    #
    # NSCP_SKIP_DOCKER=1 by default: the container-backed suites (mysql,
    # docker, ...) need a working docker daemon. Set NSCP_SKIP_DOCKER= to
    # include them.
    (
        cd tests
        NSCP_SKIP_DOCKER="${NSCP_SKIP_DOCKER-1}" \
        NSCP_BIN="$BUILD_ABS/nscp" \
            npx jest --runInBand ${JEST_ARGS:-}
    ) || { test_status=1; echo "!!! integration tests failed - collecting coverage anyway"; }
    echo "==> Collecting integration coverage"
    gcovr "${gcovr_common[@]}" --json "$ROOT/$OUT_DIR/integration.json"
    gcovr --root "$ROOT" --add-tracefile "$ROOT/$OUT_DIR/integration.json" \
        "${gcovr_report_excludes[@]}" \
        --html "$ROOT/$OUT_DIR/integration.html" \
        --print-summary
fi

echo "==> Merging reports"
merge_args=()
for f in "$OUT_DIR/unit.json" "$OUT_DIR/integration.json"; do
    [ -f "$f" ] && merge_args+=(--add-tracefile "$ROOT/$f")
done
mkdir -p "$OUT_DIR/html"
gcovr --root "$ROOT" "${merge_args[@]}" \
    "${gcovr_report_excludes[@]}" \
    --html-details "$ROOT/$OUT_DIR/html/index.html" \
    --cobertura "$ROOT/$OUT_DIR/cobertura.xml" \
    --cobertura-pretty \
    --print-summary

echo
echo "Report: $OUT_DIR/html/index.html"
if [ "$test_status" != 0 ]; then
    echo "NOTE: at least one test suite failed - the report above covers only what ran." >&2
fi
exit "$test_status"
