#!/bin/sh
set -e
CONFIG_FILE="/nsca/nsca.cfg"

cp /etc/nsca.cfg $CONFIG_FILE

mkdir -p /nsca
touch /nsca/results.txt
chown nagios:nagios /nsca /nsca/results.txt

# Print config and permissions for diagnostics
echo ">> NSCA config file ($CONFIG_FILE):"
cat $CONFIG_FILE

# Configure Encryption Method
if [ ! -z "$ENCRYPTION_METHOD" ]; then
    echo "   Setting decryption_method to $ENCRYPTION_METHOD"
    sed -i "s/^decryption_method=.*/decryption_method=$ENCRYPTION_METHOD/" $CONFIG_FILE
fi

# Configure Password
if [ ! -z "$PASSWORD" ]; then
    echo "   Setting password"
    sed -i "s/^#password=.*/password=$PASSWORD/" $CONFIG_FILE
fi

# Ensure permissions are correct
chown nagios:nagios /var/run/nsca

# Start NSCA and capture error output
echo ">> Starting NSCA Server..."
/usr/sbin/nsca -c $CONFIG_FILE --daemon 2>&1 | tee /tmp/nsca_startup.log

sleep 2

if pgrep "nsca" > /dev/null; then
    echo ">> NSCA is running on port 5667. Tailing syslog."
    tail -F /var/log/syslog
else
    echo "!! NSCA failed to start. See error output below:"
    cat /tmp/nsca_startup.log
    echo "Last 20 lines of syslog:"
    tail -20 /var/log/syslog
    exit 1
fi