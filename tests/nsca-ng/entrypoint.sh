#!/bin/sh
set -e

CONFIG_DIR="/nsca-ng"
CONFIG_FILE="$CONFIG_DIR/nsca-ng.cfg"
RESULTS_FILE="$CONFIG_DIR/results.txt"

mkdir -p "$CONFIG_DIR"
: > "$RESULTS_FILE"
chmod 0666 "$RESULTS_FILE"

# Write nsca-ng server configuration.
#
# Notes:
# - `command` is invoked once per accepted check result; the result line is
#   piped to its standard input. Use `tee -a` so the test can `findstr` the
#   results file on the host and the line is also visible in the container
#   log for debugging. (Previously this used `echo $@` which never sees the
#   input — that path silently produced an empty results file.)
# - `tls_ciphers` is set explicitly so the test stays insensitive to changes
#   in the GnuTLS NORMAL priority string. We keep the legacy PSK ciphers
#   that NSClient++ negotiates (TLS 1.2 PSK with AES-CBC).
# - `chroot = ""` disables chroot; the binary refuses to start without the
#   directive being present even when empty.
cat > "$CONFIG_FILE" <<EOF
listen      = "*:5668"
chroot      = ""
pid_file    = "/var/run/nsca-ng/nsca-ng.pid"
command     = "/bin/sh -c 'tee -a ${RESULTS_FILE}'"
tls_ciphers = "NORMAL:+PSK:-DHE-PSK:-ECDHE-PSK:%SERVER_PRECEDENCE"
EOF

# Configure password / identity (PSK).
if [ -n "$NSCA_NG_PASSWORD" ] && [ -n "$NSCA_NG_IDENTITY" ]; then
    cat >> "$CONFIG_FILE" <<EOF

authorize "${NSCA_NG_IDENTITY}" {
    password = "${NSCA_NG_PASSWORD}"
}
EOF
fi

echo ">> NSCA-NG config file ($CONFIG_FILE):"
cat "$CONFIG_FILE"

echo ">> Starting NSCA-NG Server (foreground)..."
# `-f` keeps the process in the foreground so docker can manage its lifetime;
# stdout/stderr go to docker logs for diagnostic visibility on test failure.
exec nsca-ng -c "$CONFIG_FILE" -f
