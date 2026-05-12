#!/bin/bash
# ----------------------------------------------------------------------------
# Configure Icinga 2 for the test:
#  1. (as root) Pre-create runtime dirs and hand ownership of /etc/icinga2
#     + /var/lib/icinga2 + /run/icinga2 to the `nagios` user, then re-exec
#     this script as `nagios`. icinga2's pki/cli commands internally drop
#     privileges to the daemon user; calling them as root means they
#     can't re-read the 0600 key files they just wrote. Per-command
#     `runuser -u nagios` worked for the CA step then mysteriously hung
#     the next `icinga2 pki new-cert` in detached `docker run -d` mode
#     (no TTY), so we do the privilege drop exactly once via `exec`.
#  2. (as nagios) Build a self-signed CA + node cert with `icinga2 pki`
#     (one call per step) instead of `icinga2 api setup`. `api setup` is
#     interactive in some versions and silently stalls after generating
#     the CSR when run from a container.
#  3. Write the ApiListener config + our ApiUser definition + the local
#     Endpoint/Zone that Icinga 2 ≥ 2.13 requires whenever ApiListener
#     is configured.
#  4. Drop the default sample hosts/services and write our deterministic
#     test fixture.
#  5. exec `icinga2 daemon` in the foreground.
# ----------------------------------------------------------------------------
set -e

NODE_NAME="icinga2-test"
CERT_DIR="/var/lib/icinga2/certs"

if [ "$(id -u)" = "0" ]; then
    echo ">> Preparing runtime dirs (as root)..."
    # /run/icinga2 holds the pid file at runtime. Debian's icinga2 package
    # creates it via tmpfiles.d on real boot — in a container nobody else
    # does, so the daemon aborts with "Could not open PID file
    # '/run/icinga2/icinga2.pid'." Make it ourselves.
    mkdir -p /run/icinga2 /var/lib/icinga2/ca "$CERT_DIR"
    chown -R nagios:nagios /var/lib/icinga2 /etc/icinga2 /run/icinga2

    # Re-exec as nagios via setpriv. We originally used `runuser -u nagios
    # -- "$0"` here, which works fine for `docker run` in the foreground
    # but mysteriously hangs the second `icinga2 pki` invocation under
    # `docker run -d`. setpriv is a much thinner tool — it only changes
    # uid/gid/capabilities, no PAM, no sessions, no TTY handling — so it
    # avoids whatever stdio interaction was tripping runuser in
    # detached mode.
    exec setpriv --reuid=nagios --regid=nagios --init-groups -- "$0" "$@"
fi

# ============================================================================
# Everything below this point runs as the `nagios` user.
# ============================================================================

echo ">> Enabling api feature..."
icinga2 feature enable api

echo ">> Generating CA + node certificate..."
icinga2 pki new-ca
icinga2 pki new-cert \
    --cn "$NODE_NAME" \
    --key "$CERT_DIR/$NODE_NAME.key" \
    --csr "$CERT_DIR/$NODE_NAME.csr"
icinga2 pki sign-csr \
    --csr "$CERT_DIR/$NODE_NAME.csr" \
    --cert "$CERT_DIR/$NODE_NAME.crt"
cp /var/lib/icinga2/ca/ca.crt "$CERT_DIR/ca.crt"

# Debian's stock constants.conf doesn't declare NodeName/ZoneName, so the
# daemon falls back to gethostname() (a random docker id). Pin both to
# our cert CN so the default cert lookup ("/var/lib/icinga2/certs/
# $NodeName.crt") hits the files we generated above. Appending is safe:
# this file has no pre-existing NodeName line to conflict with.
cat >> /etc/icinga2/constants.conf <<EOF

const NodeName = "$NODE_NAME"
const ZoneName = "$NODE_NAME"
EOF

# Icinga 2.13+ refuses to start the ApiListener unless an Endpoint object
# exists for the local node and that endpoint is a member of some Zone.
# Single-node setup: one Endpoint, one Zone containing only that endpoint.
cat > /etc/icinga2/zones.conf <<EOF
object Endpoint NodeName {
}
object Zone ZoneName {
  endpoints = [ NodeName ]
}
EOF

cat > /etc/icinga2/features-available/api.conf <<EOF
object ApiListener "api" {
  // Pin bind_host explicitly: some Icinga 2 builds default to "::" which
  // listens on IPv6 only, and docker's "-p 5665:5665" port-forward
  // wouldn't see traffic. 0.0.0.0 keeps the listener on IPv4.
  bind_host = "0.0.0.0"
  bind_port = "5665"
  accept_commands = true
  accept_config = true
}
EOF

cat > /etc/icinga2/conf.d/api-users.conf <<EOF
object ApiUser "${ICINGA_API_USER}" {
  password = "${ICINGA_API_PASSWORD}"
  permissions = [
    "status/query",
    "actions/process-check-result",
    "objects/query/Host",
    "objects/query/Service",
    "objects/create/Host",
    "objects/create/Service"
  ]
}
EOF

# Drop Debian's stock sample definitions. Their `assign where host.address`
# apply rules would happily attach to our test-host and run active checks
# against it.
rm -f /etc/icinga2/conf.d/hosts.conf \
      /etc/icinga2/conf.d/services.conf \
      /etc/icinga2/conf.d/users.conf \
      /etc/icinga2/conf.d/groups.conf \
      /etc/icinga2/conf.d/notifications.conf

cat > /etc/icinga2/conf.d/test-objects.conf <<'EOF'
// Passive-only template: active checks off, dummy check_command so we have
// somewhere to attach perfdata / state without Icinga 2 ever trying to run a
// real plugin.
template Service "passive-dummy" {
  import "generic-service"
  check_command = "dummy"
  enable_active_checks = false
  enable_passive_checks = true
  vars.dummy_state = 3
  vars.dummy_text = "pending"
}

object Host "test-host" {
  import "generic-host"
  address = "127.0.0.1"
  check_command = "dummy"
  enable_active_checks = false
  enable_passive_checks = true
  vars.dummy_state = 3
  vars.dummy_text = "pending"
}

// One service per block — Icinga 2's config parser requires a newline or
// semicolon between `import` and `host_name`, so the inline `{ import ...
// host_name = ... }` shorthand does not parse.
object Service "basic-ok" {
  import "passive-dummy"
  host_name = "test-host"
}
object Service "code-warn" {
  import "passive-dummy"
  host_name = "test-host"
}
object Service "code-crit" {
  import "passive-dummy"
  host_name = "test-host"
}
object Service "code-unk" {
  import "passive-dummy"
  host_name = "test-host"
}
object Service "perf-svc" {
  import "passive-dummy"
  host_name = "test-host"
}
object Service "semi-svc" {
  import "passive-dummy"
  host_name = "test-host"
}
EOF

echo ">> Validating config..."
icinga2 daemon -C

echo ">> Starting Icinga 2 (foreground)..."
exec icinga2 daemon
