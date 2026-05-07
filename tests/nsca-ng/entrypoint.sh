#!/bin/sh
set -e

CONFIG_DIR="/nsca-ng"
CONFIG_FILE="$CONFIG_DIR/nsca-ng.cfg"
RESULTS_FILE="$CONFIG_DIR/results.txt"
CMD_FIFO="/tmp/nsca-ng-cmd.fifo"

mkdir -p "$CONFIG_DIR"
: > "$RESULTS_FILE"
chmod 0666 "$RESULTS_FILE"

# nsca-ng writes accepted commands to `command_file`, which is expected to be
# Icinga's external-command FIFO. We create a real FIFO and run a background
# loop that copies everything written to it into results.txt — that way the
# host-side test can findstr the regular file. The `while true` wrap means a
# transient close (nsca-ng may close between writes) doesn't end the capture.
rm -f "$CMD_FIFO"
mkfifo "$CMD_FIFO"
chmod 0666 "$CMD_FIFO"
( while true; do cat "$CMD_FIFO" >> "$RESULTS_FILE"; done ) &

# Notes on the config:
# - `command_file` is the FIFO above (nsca-ng-server uses Icinga's external-
#   command pipe convention; there is no `command =` shell-pipe directive).
# - `tls_ciphers` is an OpenSSL cipher list string (this nsca-ng links
#   against OpenSSL 3, not GnuTLS — GnuTLS priority strings like
#   "NORMAL:+PSK" are rejected with "Cannot set SSL cipher(s)").
#   The list mirrors the agent's PSK cipher preference (AEAD first, CBC
#   fallback). `@SECLEVEL=0` is required because OpenSSL 3's default
#   SECLEVEL=1 strips the PSK CBC suites we still want for back-compat.
# - The authorize block needs an explicit pattern (`hosts`/`services`/
#   `commands`); without one, nsca-ng accepts the PSK handshake but rejects
#   every submission with FAIL.
cat > "$CONFIG_FILE" <<EOF
listen       = "*:5668"
pid_file     = "/var/run/nsca-ng/nsca-ng.pid"
command_file = "$CMD_FIFO"
tls_ciphers  = "@SECLEVEL=0:PSK-AES256-GCM-SHA384:PSK-AES128-GCM-SHA256:PSK-CHACHA20-POLY1305:PSK-AES256-CBC-SHA256:PSK-AES128-CBC-SHA256:PSK-AES256-CBC-SHA:PSK-AES128-CBC-SHA"
EOF

if [ -n "$NSCA_NG_PASSWORD" ] && [ -n "$NSCA_NG_IDENTITY" ]; then
    cat >> "$CONFIG_FILE" <<EOF

authorize "${NSCA_NG_IDENTITY}" {
    password = "${NSCA_NG_PASSWORD}"
    commands = ".*"
}
EOF
fi

echo ">> NSCA-NG config file ($CONFIG_FILE):"
cat "$CONFIG_FILE"

echo ">> Starting NSCA-NG Server (foreground)..."
# `-F` keeps the process attached to the controlling terminal so docker can
# manage its lifetime; stdout/stderr go to docker logs for diagnostic
# visibility on test failure. (Lowercase `-f` is not a valid nsca-ng flag —
# the binary exits with the usage banner and the container terminates,
# producing a confusing ECONNREFUSED at the client.) Add `-l 7` here when
# debugging a TLS handshake or auth issue.
exec nsca-ng -c "$CONFIG_FILE" -F
