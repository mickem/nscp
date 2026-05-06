#!/bin/sh
set -e

CONFIG_DIR="/nsca-ng"
CONFIG_FILE="$CONFIG_DIR/nsca-ng.cfg"
RESULTS_FILE="$CONFIG_DIR/results.txt"

mkdir -p "$CONFIG_DIR"
touch "$RESULTS_FILE"

# Write nsca-ng server configuration
cat > "$CONFIG_FILE" <<EOF
# NSCA-NG server configuration for integration tests
listen      = "*:5668"
chroot      = ""
# Write received check results to a file (one per line, Nagios external command format)
command     = "/bin/sh -c 'echo \$@ >> ${RESULTS_FILE}'"
EOF

# Configure password / identity
if [ -n "$NSCA_NG_PASSWORD" ] && [ -n "$NSCA_NG_IDENTITY" ]; then
    cat >> "$CONFIG_FILE" <<EOF

authorize "${NSCA_NG_IDENTITY}" {
    password = "${NSCA_NG_PASSWORD}"
}
EOF
fi

echo ">> NSCA-NG config file ($CONFIG_FILE):"
cat "$CONFIG_FILE"

echo ">> Starting NSCA-NG Server..."
nsca-ng -c "$CONFIG_FILE" -f 2>&1 &
SERVER_PID=$!

sleep 2

if kill -0 "$SERVER_PID" 2>/dev/null; then
    echo ">> NSCA-NG is running on port 5668 (PID $SERVER_PID)."
    wait "$SERVER_PID"
else
    echo "!! NSCA-NG failed to start."
    exit 1
fi
