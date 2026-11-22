#!/bin/bash
# fleet-icinga-agent-sync - make every enrolled fleet host an Icinga 2 agent.
#
# The agent-mode counterpart of the passive flow, where agents create their
# own objects through the REST API. An agent the master schedules checks on
# cannot do that: the master has to hold its Endpoint, Zone and Host first, and
# has to sign its certificate. This script does both, once a minute:
#
#   1. polls GET /api/hosts on an NSClient fleet server and renders one config
#      file per enrolled host - Endpoint + Zone (child of the master's zone) +
#      Host. The services come from apply rules that match these hosts
#      (fleet-agent-services.conf, written by setup-icinga-agent-sync.ps1).
#      When the rendered tree differs from the live one it is swapped in,
#      validated with `icinga2 daemon -C` and Icinga is reloaded; a validation
#      failure rolls the previous tree back.
#   2. signs the pending certificate requests (`icinga2 ca sign`) of exactly
#      those hosts. Tickets are per host and a fleet bundle is the same for
#      every host, so the agents ask without one.
#
# A fleet server that does not answer leaves everything untouched.
#
# Step 2 means "enrolled with the fleet server under this host name" is all it
# takes to be signed as that host. Fine for a throwaway test estate; a real
# master hands out tickets or signs by hand.
#
# Installed by setup-icinga-agent-sync.ps1 together with a systemd timer; the
# config below normally comes from /etc/fleet-icinga-agent-sync.conf (root:root
# 0600, it holds the API key). A view_only fleet API key is all this needs.
#
# Test mode (no fleet server, no root, no Icinga):
#   fleet-icinga-agent-sync.sh --hosts-file hosts.json --render-only out-dir/
set -u

CONF_FILE="${FLEET_ICINGA_AGENT_SYNC_CONF:-/etc/fleet-icinga-agent-sync.conf}"

# Defaults (Ubuntu icinga2 layout), overridable from the environment and, with
# higher precedence, from the conf file.
FLEET_SERVER="${FLEET_SERVER:-}"
FLEET_API_KEY="${FLEET_API_KEY:-}"
CURL_OPTS="${CURL_OPTS:-}"
ICINGA_ETC="${ICINGA_ETC:-/etc/icinga2}"
FLEET_CONF_DIR="${FLEET_CONF_DIR:-/etc/icinga2/conf.d/fleet-agents}"
API_PACKAGE_DIR="${API_PACKAGE_DIR:-/var/lib/icinga2/api/packages/_api}"
ICINGA_BIN="${ICINGA_BIN:-icinga2}"
ICINGA_USER="${ICINGA_USER:-nagios}"
RELOAD_CMD="${RELOAD_CMD:-systemctl reload icinga2}"
# What to do with a host the passive flow left behind as an API-created object
# of the same name (it would be a duplicate): 1 deletes it through the API
# below, anything else skips the host.
REPLACE_API_HOSTS="${REPLACE_API_HOSTS:-0}"
ICINGA_API="${ICINGA_API:-https://localhost:5665/v1}"
ICINGA_API_AUTH="${ICINGA_API_AUTH:-}"

HOSTS_FILE=""
RENDER_ONLY=""
while [ $# -gt 0 ]; do
    case "$1" in
        --hosts-file)  HOSTS_FILE="$2"; shift 2 ;;
        --render-only) RENDER_ONLY="$2"; shift 2 ;;
        *) echo "fleet-icinga-agent-sync: unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [ -r "$CONF_FILE" ]; then
    # shellcheck source=/dev/null
    . "$CONF_FILE"
fi

fail() { echo "fleet-icinga-agent-sync: $*" >&2; exit 1; }
note() { echo "fleet-icinga-agent-sync: $*"; }

command -v jq >/dev/null 2>&1 || fail "jq is required"

# --- fetch ------------------------------------------------------------------
if [ -n "$HOSTS_FILE" ]; then
    hosts_json=$(cat "$HOSTS_FILE") || fail "cannot read $HOSTS_FILE"
else
    [ -n "$FLEET_SERVER" ] || fail "FLEET_SERVER is not set (edit $CONF_FILE)"
    [ -n "$FLEET_API_KEY" ] || fail "FLEET_API_KEY is not set (edit $CONF_FILE)"
    # shellcheck disable=SC2086  # CURL_OPTS is intentionally word-split
    hosts_json=$(curl -fsS --max-time 30 $CURL_OPTS \
        -H "Authorization: Bearer $FLEET_API_KEY" \
        "${FLEET_SERVER%/}/api/hosts") \
        || fail "could not fetch ${FLEET_SERVER%/}/api/hosts - keeping the current Icinga config"
fi

echo "$hosts_json" | jq -e 'type == "array"' >/dev/null 2>&1 \
    || fail "the fleet server did not return a host array - keeping the current Icinga config"

# --- render -----------------------------------------------------------------
# Enrolled hosts only (enrolled_at is how the server itself defines it).
# hostname\tid\tos, tab-separated.
enrolled=$(echo "$hosts_json" | jq -r '
    .[]
    | select(.hostname != null and .enrolled_at != null)
    | [.hostname, .id, (.os // "unknown")] | @tsv')

tmp_dir=$(mktemp -d) || fail "mktemp failed"
trap 'rm -rf "$tmp_dir"' EXIT
render_dir="$tmp_dir/fleet-agents"
mkdir -p "$render_dir"
# The endpoint names this run manages, one per line - the only certificate
# requests it will sign.
managed="$tmp_dir/managed"
: > "$managed"

# Is an object of this name defined by somebody else's config file? A second
# definition fails validation for the whole tree, so such a host is left to
# whoever wrote that file.
defined_elsewhere() {
    [ -d "$ICINGA_ETC" ] || return 1
    grep -rlE --include='*.conf' "^[[:space:]]*object[[:space:]]+(Endpoint|Zone|Host)[[:space:]]+\"($1|$2)\"" "$ICINGA_ETC" 2>/dev/null \
        | grep -v -F "$FLEET_CONF_DIR/" | head -n 1
}

count=0
while IFS=$'\t' read -r hostname host_id host_os; do
    [ -n "$hostname" ] || continue
    # The hostname becomes a file name and an Icinga object name; anything
    # outside this alphabet is either malice or breakage - skip it loudly.
    case "$hostname" in
        (*[!A-Za-z0-9._-]*|.*|-*)
            echo "fleet-icinga-agent-sync: skipping host with unsafe hostname: $hostname (fleet id $host_id)" >&2
            continue ;;
    esac
    # The agent's endpoint name defaults to its lower-cased host name, and that
    # is what its certificate is issued for.
    endpoint=$(printf '%s' "$hostname" | tr '[:upper:]' '[:lower:]')
    host_os=$(printf '%s' "$host_os" | tr -cd 'A-Za-z0-9._ -')

    if [ -z "$RENDER_ONLY" ]; then
        other=$(defined_elsewhere "$hostname" "$endpoint")
        if [ -n "$other" ]; then
            note "skipping $hostname: already defined in $other"
            continue
        fi
        api_host=$(ls "$API_PACKAGE_DIR"/*/conf.d/hosts/"$hostname".conf 2>/dev/null | head -n 1)
        if [ -n "$api_host" ]; then
            if [ "$REPLACE_API_HOSTS" = "1" ] && [ -n "$ICINGA_API_AUTH" ]; then
                code=$(curl -ks -o /dev/null -w '%{http_code}' -u "$ICINGA_API_AUTH" -X DELETE \
                    -H 'Accept: application/json' "$ICINGA_API/objects/hosts/$hostname?cascade=1" || true)
                case "$code" in
                    2*) note "removed the passive flow's host object $hostname (and its services)" ;;
                    *)  note "skipping $hostname: could not remove its API-created host object (HTTP $code)"
                        continue ;;
                esac
            else
                note "skipping $hostname: it exists as an API-created host (the passive flow's); delete it or install with -ReplacePassive"
                continue
            fi
        fi
    fi

    {
        echo "// Generated by fleet-icinga-agent-sync - DO NOT EDIT (fleet id $host_id, os $host_os)"
        echo "object Endpoint \"$endpoint\" { }"
        echo ""
        echo "object Zone \"$endpoint\" {"
        echo "  endpoints = [ \"$endpoint\" ]"
        echo "  parent = ZoneName"
        echo "}"
        echo ""
        echo "object Host \"$hostname\" {"
        echo "  import \"fleet-agent-host\""
        echo "  address = \"$hostname\""
        echo "  vars.agent_endpoint = \"$endpoint\""
        echo "  vars.cluster_zone = \"$endpoint\""
        echo "  vars.fleet_os = \"$host_os\""
        echo "}"
    } > "$render_dir/$endpoint.conf"
    echo "$endpoint" >> "$managed"
    count=$((count + 1))
done <<EOF
$enrolled
EOF

if [ -n "$RENDER_ONLY" ]; then
    mkdir -p "$RENDER_ONLY"
    rm -rf "${RENDER_ONLY:?}"/*.conf
    cp "$render_dir"/*.conf "$RENDER_ONLY"/ 2>/dev/null
    note "rendered $count host(s) into $RENDER_ONLY"
    exit 0
fi

# One writer at a time: the timer and a hand-run (or the provisioning script)
# must not interleave their rm/cp/validate steps on the live tree.
lock_file="${LOCK_FILE:-/run/lock/fleet-icinga-agent-sync.lock}"
if command -v flock >/dev/null 2>&1; then
    exec 9>"$lock_file" || fail "cannot open lock $lock_file"
    flock -w 120 9 || fail "another fleet-icinga-agent-sync is still running"
fi

# --- swap + validate + reload ------------------------------------------------
mkdir -p "$FLEET_CONF_DIR"
if diff -r -q "$FLEET_CONF_DIR" "$render_dir" >/dev/null 2>&1; then
    note "no config change ($count host(s))"
else
    # The previous tree goes back whenever anything after this point fails:
    # a tree that does not validate would stop the next icinga2 restart.
    backup_dir="$tmp_dir/previous"
    cp -a "$FLEET_CONF_DIR" "$backup_dir" || fail "could not back up $FLEET_CONF_DIR"
    restore_previous() {
        rm -rf "${FLEET_CONF_DIR:?}"/*.conf
        cp -a "$backup_dir"/. "$FLEET_CONF_DIR"/ || echo "fleet-icinga-agent-sync: RESTORE FAILED - $FLEET_CONF_DIR may be incomplete" >&2
    }
    rm -rf "${FLEET_CONF_DIR:?}"/*.conf
    if ! cp -a "$render_dir"/. "$FLEET_CONF_DIR"/; then
        restore_previous
        fail "could not write into $FLEET_CONF_DIR - restored the previous tree"
    fi
    chmod 755 "$FLEET_CONF_DIR"
    chmod 644 "$FLEET_CONF_DIR"/*.conf 2>/dev/null

    # Capture the validation output before rolling back - re-running it
    # afterwards would validate the restored (good) tree and report nothing.
    if ! validate_out=$("$ICINGA_BIN" daemon -C 2>&1); then
        echo "$validate_out" | grep -E 'critical|Error|error' | head -n 20 >&2
        restore_previous
        fail "the rendered config failed icinga2 validation - rolled back"
    fi

    $RELOAD_CMD || fail "icinga2 reload failed"
    note "applied $count host(s) and reloaded icinga2"
fi

# --- sign --------------------------------------------------------------------
# As the daemon user: `ca sign` rewrites the request file, and one left behind
# owned by root is unreadable to the daemon that has to hand the certificate
# out. setpriv, not runuser - no PAM, nothing that waits on a terminal.
as_icinga() {
    timeout 60 setpriv --reuid="$ICINGA_USER" --regid="$ICINGA_USER" --init-groups -- "$@" </dev/null
}
# `icinga2 ca list` prints "fingerprint | timestamp | signed | subject" and
# only the requests still waiting; the subject ends in "CN = <endpoint>".
signed=0
while IFS='|' read -r fingerprint _ is_signed subject; do
    fingerprint=$(echo "$fingerprint" | tr -d '[:space:]')
    case "$fingerprint" in (*[!0-9a-fA-F]*|"") continue ;; esac
    [ -z "$(echo "$is_signed" | tr -d '[:space:]')" ] || continue
    cn=$(echo "$subject" | sed -n 's/.*CN = \([^,]*\).*/\1/p' | tr -d '[:space:]"' | tr '[:upper:]' '[:lower:]')
    [ -n "$cn" ] || continue
    grep -qxF "$cn" "$managed" || continue
    if as_icinga "$ICINGA_BIN" ca sign "$fingerprint" >/dev/null 2>&1; then
        note "signed the certificate request of $cn"
        signed=$((signed + 1))
    else
        echo "fleet-icinga-agent-sync: could not sign the request of $cn ($fingerprint)" >&2
    fi
done <<EOF
$(as_icinga "$ICINGA_BIN" ca list 2>/dev/null)
EOF
[ "$signed" -eq 0 ] || note "signed $signed certificate request(s)"
exit 0
