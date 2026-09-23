#!/bin/sh
# Start the shipped systemd unit for real and check that it gives the agent the
# host's own view of the mount table.
#
# This is the regression that matters for a monitoring agent. ProtectSystem,
# ProtectHome and PrivateTmp are each implemented as bind mounts that land in
# the service's /proc/mounts, and check_drivesize enumerates that file skipping
# only pseudo filesystems and repeated mount points (check_drive_unix.cpp). A
# ProtectSystem bind carries the root filesystem's own type and a fresh mount
# point, so it survives both filters and becomes an extra drive reporting / 's
# usage with writable = 0 - and check_disk_io_unix.cpp applies the identical
# filter, so the same phantom rows reach perfdata and OpenMetrics. Adding any
# of those three directives back would therefore corrupt what the agent
# reports, silently and on every Linux host. This test fails if one reappears.
#
# Nothing else in CI loads the unit: the Debian and RedHat jobs build in a
# container and drive `nscp test` directly. Starting it needs systemd as PID 1
# (a GitHub `ubuntu-*` runner is one; a container is not), so the start cases
# are skipped there while the render and verify still run - that is where this
# script's own bugs live.
set -eu

UNIT_IN=${1:-files/nsclient.service.in}
NAME=nscp-unit-test
UNIT_PATH=/etc/systemd/system/${NAME}.service
PROBE_PATH=/usr/local/lib/${NAME}-probe.sh
WORK=/var/lib/${NAME}

if ! command -v systemd-analyze >/dev/null 2>&1 || [ "$(id -u)" != "0" ]; then
  echo "SKIP: needs systemd-analyze and root." >&2
  exit 0
fi

CAN_START=1
if [ "$(ps -p 1 -o comm=)" != "systemd" ]; then
  echo "NOTE: systemd is not PID 1 here; rendering and verifying only." >&2
  CAN_START=0
fi

cleanup() {
  systemctl stop ${NAME}.service >/dev/null 2>&1 || true
  rm -f "$UNIT_PATH" "$PROBE_PATH"
  systemctl daemon-reload >/dev/null 2>&1 || true
  rm -rf "$WORK"
  userdel "$NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT

id "$NAME" >/dev/null 2>&1 || useradd --system --no-create-home --shell /usr/sbin/nologin "$NAME"
mkdir -p "$WORK" && chown "$NAME" "$WORK"

# What the host sees. The service must see no mount point that is not here.
awk '{print $2}' /proc/self/mounts | sort -u > "$WORK/host-mounts"
chown "$NAME" "$WORK/host-mounts"

cat > "$PROBE_PATH" <<PROBE
#!/bin/sh
awk '{print \$2}' /proc/self/mounts | sort -u > "$WORK/service-mounts"
echo "\$(id -un)" > "$WORK/service-user"
echo PROBE_DONE
PROBE
chmod 0755 "$PROBE_PATH"

render_unit() {
  # The real unit, with a one-shot stand-in for ExecStart and a test account.
  sed \
    -e "s#@NSCP_PKGLIBDIR@#/tmp#" \
    -e "s#@NSCP_PKGSTATEDIR@#${WORK}#" \
    -e "s#@NSCP_LOGDIR@#${WORK}#" \
    -e "s#@NSCP_SBINDIR@#/usr/sbin#" \
    -e "/^ExecStart=/d" \
    -e "s#^User=.*#User=${NAME}#" \
    -e "s#^WorkingDirectory=.*#WorkingDirectory=/tmp#" \
    "$UNIT_IN" > "$UNIT_PATH"
  {
    echo "[Service]"
    echo "Type=oneshot"
    echo "ExecStart=${PROBE_PATH}"
  } >> "$UNIT_PATH"
  [ "$CAN_START" = "1" ] && systemctl daemon-reload

  # Verify what was rendered, not the template: systemd-analyze takes a unit
  # *name* and refuses "nsclient.service.in" with "Failed to prepare
  # filename ...: Invalid argument".
  PROBLEMS=$(systemd-analyze verify "$UNIT_PATH" 2>&1 || true)
  if [ -n "$PROBLEMS" ]; then
    echo "FAIL: systemd-analyze reported a problem with the unit"
    echo "$PROBLEMS"
    exit 1
  fi
  echo "   unit verifies"
}

echo "== the unit declares no mount-namespace directive"
NS=$(grep -nE '^\s*(ProtectSystem|ProtectHome|PrivateTmp|PrivateDevices|ReadWritePaths|ReadOnlyPaths|InaccessiblePaths|TemporaryFileSystem|RootDirectory|MountAPIVFS)=' "$UNIT_IN" || true)
if [ -n "$NS" ]; then
  echo "FAIL: the unit sets a directive that gives the service its own mount"
  echo "      namespace. Those appear in the service's /proc/mounts and become"
  echo "      phantom drives in check_drivesize and the disk-free collector."
  echo "$NS"
  exit 1
fi
echo "   ok"

render_unit

echo "== the service sees the host's mount table"
if [ "$CAN_START" != "1" ]; then
  echo "   skipped (no systemd), unit verified above"
  echo "All service-unit checks passed."
  exit 0
fi

systemctl reset-failed ${NAME}.service >/dev/null 2>&1 || true
if ! systemctl start ${NAME}.service; then
  echo "FAIL: the unit did not start"
  systemctl status ${NAME}.service --no-pager -l || true
  journalctl -u ${NAME}.service --no-pager -n 40 || true
  exit 1
fi
if ! journalctl -u ${NAME}.service --no-pager -n 40 | grep -q PROBE_DONE; then
  echo "FAIL: the probe did not run"
  journalctl -u ${NAME}.service --no-pager -n 40 || true
  exit 1
fi

if [ "$(cat "$WORK/service-user")" != "$NAME" ]; then
  echo "FAIL: the service did not run as $NAME"
  exit 1
fi

EXTRA=$(comm -13 "$WORK/host-mounts" "$WORK/service-mounts")
if [ -n "$EXTRA" ]; then
  echo "FAIL: the service sees mount points the host does not. check_drivesize"
  echo "      would report these as extra drives:"
  echo "$EXTRA"
  exit 1
fi
echo "   ok ($(wc -l < "$WORK/service-mounts") mount points, same as the host)"

echo "All service-unit checks passed."
