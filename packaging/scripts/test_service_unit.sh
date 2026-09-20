#!/bin/sh
# Start the shipped systemd unit's sandbox for real and check what it allows.
#
# Nothing else in CI does: the Debian and RedHat jobs build in a container and
# drive `nscp test` / `nscp client` directly, so no job installs the package and
# starts nsclient.service. A directive that stops the service starting at all -
# an unprefixed ReadWritePaths naming a directory that is not there fails the
# unit with "Failed to set up mount namespacing" - would ship with every check
# green.
#
# This runs the real directives from files/nsclient.service.in with a stand-in
# ExecStart, so it needs systemd as PID 1 (a GitHub `ubuntu-*` runner is one; a
# container generally is not, and the script skips there). It does not test the
# package: it covers the two things that matter most for the unit itself - that
# it starts, and that the sandbox permits and denies the paths it should.
set -eu

UNIT_IN=${1:-files/nsclient.service.in}
NAME=nscp-sandbox-test
UNIT_PATH=/etc/systemd/system/${NAME}.service
PROBE_PATH=/usr/local/lib/${NAME}-probe.sh

STATE_DIR=/var/lib/${NAME}
LOG_DIR=/var/log/${NAME}
DATA_DIR=/srv/${NAME}

if [ "$(ps -p 1 -o comm=)" != "systemd" ]; then
  echo "SKIP: systemd is not PID 1 here, cannot start a unit." >&2
  exit 0
fi

cleanup() {
  systemctl stop ${NAME}.service >/dev/null 2>&1 || true
  rm -f "$UNIT_PATH" "$PROBE_PATH"
  systemctl daemon-reload >/dev/null 2>&1 || true
  rm -rf "$STATE_DIR" "$LOG_DIR" "$DATA_DIR"
}
trap cleanup EXIT

id "$NAME" >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin "$NAME"

# The probe, as a file rather than an inline ExecStart: every assertion the
# sandbox should make, as an exit code. `check_disk_write` is a
# create-write-fsync-read-delete round trip, and a plain create is the part of
# it a read-only mount blocks, so that is what this imitates.
cat > "$PROBE_PATH" <<'PROBE'
#!/bin/sh
fail() { echo "FAIL: $1"; exit 1; }
rw() { ( : > "$1/probe" ) 2>/dev/null || fail "$2 ($1) is not writable"; rm -f "$1/probe"; }
ro() { if ( : > "$1/probe" ) 2>/dev/null; then rm -f "$1/probe"; fail "$2 ($1) is writable"; fi; }

rw /tmp "the private /tmp"
rw "$DATA" "a data mount - ProtectSystem is too strict for check_disk_write"
ro /etc "the configuration directory"
ro /usr "the system directory"
[ "${PROBE_STATE:-1}" = "1" ] && rw "$STATE" "the state directory"
[ "${PROBE_LOGS:-1}" = "1" ] && rw "$LOGS" "the log directory"
echo SANDBOX_PROBE_OK
PROBE
chmod 0755 "$PROBE_PATH"

render_unit() {
  # The real unit, with a one-shot stand-in for ExecStart and a test account.
  sed \
    -e "s#@NSCP_PKGLIBDIR@#/tmp#" \
    -e "s#@NSCP_PKGSTATEDIR@#${STATE_DIR}#" \
    -e "s#@NSCP_LOGDIR@#${LOG_DIR}#" \
    -e "s#@NSCP_SBINDIR@#/usr/sbin#" \
    -e "/^ExecStart=/d" \
    -e "/^PIDFile=/d" \
    -e "s#^User=.*#User=${NAME}#" \
    -e "s#^WorkingDirectory=.*#WorkingDirectory=/tmp#" \
    "$UNIT_IN" > "$UNIT_PATH"
  {
    echo "[Service]"
    echo "Type=oneshot"
    echo "Environment=STATE=${STATE_DIR} LOGS=${LOG_DIR} DATA=${DATA_DIR}"
    echo "Environment=PROBE_STATE=${PROBE_STATE:-1} PROBE_LOGS=${PROBE_LOGS:-1}"
    echo "ExecStart=${PROBE_PATH}"
  } >> "$UNIT_PATH"
  systemctl daemon-reload
}

run_case() {
  echo "== $1"
  systemctl reset-failed ${NAME}.service >/dev/null 2>&1 || true
  if ! systemctl start ${NAME}.service; then
    echo "FAIL: the unit did not start"
    systemctl status ${NAME}.service --no-pager -l || true
    journalctl -u ${NAME}.service --no-pager -n 40 || true
    exit 1
  fi
  if ! journalctl -u ${NAME}.service --no-pager -n 40 | grep -q SANDBOX_PROBE_OK; then
    echo "FAIL: the sandbox probe did not pass"
    journalctl -u ${NAME}.service --no-pager -n 40 || true
    exit 1
  fi
  echo "   ok"
}

echo "== systemd-analyze verify"
# The only expected complaint is the absent ExecStart binary on a host where
# nothing is installed; anything else is a problem with the unit.
PROBLEMS=$(systemd-analyze verify "$UNIT_IN" 2>&1 | grep -v "is not executable" || true)
if [ -n "$PROBLEMS" ]; then
  echo "FAIL: systemd-analyze reported a problem with the unit"
  echo "$PROBLEMS"
  exit 1
fi
echo "   ok"

mkdir -p "$DATA_DIR" && chown "$NAME" "$DATA_DIR"

# Case 1: both directories present, as on a packaged install.
mkdir -p "$STATE_DIR" "$LOG_DIR" && chown "$NAME" "$STATE_DIR" "$LOG_DIR"
PROBE_STATE=1 PROBE_LOGS=1 render_unit
run_case "the state and log directories are present"

# Case 2: the log directory is absent, as on a source install or after someone
# cleans out /var/log. The unit must still start - an unprefixed ReadWritePaths
# fails it here - so the probe skips the log directory and only the start
# matters. Nothing recreates the directory first: systemd resolves
# ReadWritePaths when the unit starts, not when it is loaded.
systemctl stop ${NAME}.service >/dev/null 2>&1 || true
rm -rf "$LOG_DIR"
PROBE_STATE=1 PROBE_LOGS=0 render_unit
run_case "the log directory is absent at unit start"

echo "All service-unit sandbox checks passed."
