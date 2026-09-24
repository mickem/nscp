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

# --user-group so the account gets a primary group of its own, which is what
# the unit's Group= names on a real install.
id "$NAME" >/dev/null 2>&1 || useradd --system --user-group --no-create-home --shell /usr/sbin/nologin "$NAME"
mkdir -p "$WORK" && chown "$NAME" "$WORK"

# What the host sees, and the filesystem type of each mount, since that is what
# decides whether check_drivesize turns a mount into a drive row.
awk '{print $2 "\t" $3}' /proc/self/mounts | sort -u > "$WORK/host-mounts"
chown "$NAME" "$WORK/host-mounts"

# The pseudo-filesystem list is read out of the check's own source rather than
# copied here, so the two cannot drift apart: whatever check_drivesize skips,
# this skips.
sed -n '/const std::set<std::string> pseudo = {/,/};/p' modules/CheckDisk/check_drive_unix.cpp \
  | sed 's://.*::' | grep -o '"[^"]*"' | tr -d '"' | sort -u > "$WORK/pseudo-fs"
if [ ! -s "$WORK/pseudo-fs" ]; then
  echo "FAIL: could not read is_pseudo_fs() out of check_drive_unix.cpp"
  exit 1
fi

cat > "$PROBE_PATH" <<PROBE
#!/bin/sh
awk '{print \$2 "\\t" \$3}' /proc/self/mounts | sort -u > "$WORK/service-mounts"
echo "\$(id -un)" > "$WORK/service-user"
echo "\$(id -gn)" > "$WORK/service-group"
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
    -e "s#^Group=.*#Group=${NAME}#" \
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

# Checkable without systemd, so it runs everywhere this script does. The
# runtime assertion further down proves the group actually takes effect; this
# one catches the line simply going missing.
echo "== the unit pins User= and Group="
for key in User Group; do
  if ! grep -qE "^\s*${key}=" "$UNIT_IN"; then
    echo "FAIL: the unit does not set ${key}=."
    if [ "$key" = "Group" ]; then
      echo "      Without it the service inherits the account's primary group,"
      echo "      which the DEB leaves as 'nogroup'; with UMask=0027 that makes"
      echo "      everything the service writes readable by every daemon in it."
    fi
    exit 1
  fi
done
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

# The unit must pin the group, not inherit the account's primary group. Without
# Group= the DEB runs the service under `nogroup`, and UMask=0027 then leaves
# everything it writes readable by every daemon sharing that group.
if [ "$(cat "$WORK/service-group")" != "$NAME" ]; then
  echo "FAIL: the service ran with group '$(cat "$WORK/service-group")', not"
  echo "      '$NAME' - the unit is not pinning Group=, so on a packaged"
  echo "      install it would inherit the account's primary group."
  exit 1
fi
echo "   runs as $NAME:$NAME"

# Any unit gets some mount entries the host lacks - ProtectKernelTunables masks
# paths under /proc, ProtectKernelModules masks /usr/lib/modules, and systemd
# adds its own plumbing under /run and /sys/fs/cgroup. Those are harmless here
# because check_drivesize skips them by filesystem type. What must not appear is
# an extra mount whose type is NOT skipped: that is exactly a phantom drive row.
EXTRA=$(awk -F'\t' '
  NR == FNR { host[$1] = 1; next }
  !($1 in host) { print $1 "\t" $2 }
' "$WORK/host-mounts" "$WORK/service-mounts")

REPORTED=""
if [ -n "$EXTRA" ]; then
  echo "   extra mount entries in the service (type in brackets):"
  printf '%s\n' "$EXTRA" | while IFS="$(printf '\t')" read -r mp fs; do
    echo "     $mp [$fs]"
  done
  REPORTED=$(printf '%s\n' "$EXTRA" | while IFS="$(printf '\t')" read -r mp fs; do
    grep -qxF "$fs" "$WORK/pseudo-fs" || echo "$mp [$fs]"
  done)
fi

if [ -n "$REPORTED" ]; then
  echo "FAIL: the service sees mount points the host does not, on filesystem"
  echo "      types check_drivesize does NOT skip - each becomes an extra drive"
  echo "      row reporting another filesystem's usage, with writable = 0:"
  printf '%s\n' "$REPORTED"
  exit 1
fi
echo "   ok (no extra mount that check_drivesize would report as a drive)"

echo "All service-unit checks passed."
