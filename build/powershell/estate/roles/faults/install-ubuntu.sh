#!/bin/bash
# "Acme Agent" for the faults estate role, on Ubuntu: a stand-in vendor
# package whose parts misbehave on demand, so a fault the role's bundle
# scripts switch on has a name in the host's facts (software.installed,
# services.installed) and in its process list. Runs as root before
# NSClient++ is installed (see role.json and README.md, "Adding a role").
#
#   /opt/acme/bin/acme-indexer  a copy of `yes`: one per core is the CPU hog
#   /opt/acme/bin/acme-cache    a copy of `tail`: fed zeros with no newline,
#                               it holds all of them - the memory hog
#   /opt/acme/bin/acme-sync     the acme-sync service, which crash-loops while
#                               /var/lib/acme/sync.fail exists
#   /var/lib/acme/cache         where the disk filler puts its file
#
# The agent runs the fault scripts as the nsclient user, so everything a fault
# touches is writable by it: /var/lib/acme is world-writable and sticky, and
# the service needs no restart by hand - it heals itself once the trigger file
# is gone.
set -euo pipefail

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
pkg="$stage/acme-agent"
mkdir -p "$pkg/DEBIAN" "$pkg/opt/acme/bin" "$pkg/lib/systemd/system"

cp "$(command -v yes)" "$pkg/opt/acme/bin/acme-indexer"
cp "$(command -v tail)" "$pkg/opt/acme/bin/acme-cache"
cat > "$pkg/opt/acme/bin/acme-sync" <<'EOF'
#!/bin/sh
# Syncs the Acme catalogue. Exits - and so crash-loops under systemd - while
# /var/lib/acme/sync.fail exists.
while :; do
  if [ -e /var/lib/acme/sync.fail ]; then
    echo "acme-sync: cannot open /var/lib/acme/catalog.db: database disk image is malformed" >&2
    exit 1
  fi
  sleep 2
done
EOF
chmod 755 "$pkg/opt/acme/bin/"*

cat > "$pkg/lib/systemd/system/acme-sync.service" <<'EOF'
[Unit]
Description=Acme catalogue sync
# Restart forever: the faults role relies on the service coming back by
# itself once the fault is cleared.
StartLimitIntervalSec=0

[Service]
ExecStart=/opt/acme/bin/acme-sync
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

cat > "$pkg/DEBIAN/control" <<EOF
Package: acme-agent
Version: 2.3.1
Architecture: $(dpkg --print-architecture)
Maintainer: NSClient++ estate <noreply@example.invalid>
Description: Acme indexing agent
 A stand-in vendor package for the NSClient++ faults estate role. Its parts
 hog the CPU, hold memory, fill the disk and crash-loop on demand.
EOF

dpkg-deb --build --root-owner-group "$pkg" "$stage/acme-agent.deb"
dpkg -i "$stage/acme-agent.deb"

install -d -m 1777 /var/lib/acme /var/lib/acme/cache
rm -f /var/lib/acme/sync.fail
systemctl daemon-reload
systemctl enable --now acme-sync
systemctl is-active acme-sync
echo "acme-agent installed"
