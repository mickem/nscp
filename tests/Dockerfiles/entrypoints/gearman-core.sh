#!/bin/bash
# ----------------------------------------------------------------------------
# Shared entrypoint for the two "monitoring core + Mod-Gearman" test images
# (nagios-gearman.Dockerfile and naemon-gearman.Dockerfile).
#
# It brings up, inside one container:
#   1. gearmand on $GEARMAN_PORT (4730), listening on $GEARMAND_LISTEN so the
#      host-mapped port reaches it;
#   2. a minimal, self-contained core configuration under $WORK (no reliance
#      on the package's own nagios.cfg / naemon.cfg, whose paths differ per
#      distribution), with `interval_length=1` so every interval is in
#      seconds and `status_update_interval=1` so the status file is fresh;
#   3. the Mod-Gearman NEB module (ConSol's mod_gearman_naemon.o or the Nagios
#      fork's nagios-mod-gearman.o) routing every check for hostgroup
#      $GEARMAN_HOSTGROUP into the gearmand queue `hostgroup_<name>`;
#   4. the core itself, in the foreground.
#
# The object config is the one from the GearmanClient plan: one `nscp`
# command whose command line is `$ARG1$`, one host `nscp-test` in the test
# hostgroup, and three services. The worker on the other side of gearmand
# (the TypeScript stub in tests/src/gearman.ts, or the GearmanClient module
# in tests/gearman-core.test.ts) receives the fully expanded `$ARG1$` as the
# job's command_line, so every check here is written as an NSClient++ query
# an agent can actually answer:
#
#   host nscp-test  check_always_ok check_ok message=host-is-up
#                     a wrapped query, so the path proves more than one
#                     command dispatch
#   service helper  check_ok message=hello        plain query, message only
#   service cpu     check_cpu "warning=usage gt 101" …
#                     collector-backed, for the performance data - and
#                     written with `gt` rather than `>`, which the agent's
#                     metacharacter guard refuses by default
#   service slow    check_slow
#                     supplied by the worker (an external script that
#                     sleeps), so the check overruns the job's `timeout` and
#                     the agent's timeout handling is exercised against a
#                     real core
#
# The timeout the core puts in the job is service_check_timeout /
# host_check_timeout below, three seconds.
#
# Which core is driven is selected by GEARMAN_CORE (nagios|naemon); every
# path has a sensible default for the docker image and can be overridden
# through the environment, which is how the same script is run natively
# against a from-source build to regenerate the fixtures
# (see modules/GearmanClient/fixtures/capture.sh).
#
# Capture mode (CAPTURE_DIR set): instead of serving checks forever, the
# script grabs the raw job payloads the NEB module puts on the hostgroup
# queue with the `gearman` CLI in worker mode (it prints a job's workload
# verbatim), classifies them as host or service jobs by decrypting them with
# the openssl CLI, then produces result payloads with send_gearman on a side
# queue and grabs those too. It writes:
#   <core>-job-host.b64        a host check job from the NEB module
#   <core>-job-service.b64     a service check job from the NEB module
#   <core>-result-passive-service.b64   send_gearman, passive service result
#   <core>-result-passive-host.b64      send_gearman, passive host result
#   <core>-result-active-service.b64    send_gearman --active, fixed times
#   <core>-versions.txt        what produced them
# and exits 0 when all six exist.
# ----------------------------------------------------------------------------
set -eu

GEARMAN_CORE="${GEARMAN_CORE:?set GEARMAN_CORE=nagios or GEARMAN_CORE=naemon}"
GEARMAN_PORT="${GEARMAN_PORT:-4730}"
GEARMAND_LISTEN="${GEARMAND_LISTEN:-0.0.0.0}"
GEARMAN_KEY="${GEARMAN_KEY:-nscp-test-key}"
GEARMAN_ENCRYPTION="${GEARMAN_ENCRYPTION:-yes}"
GEARMAN_HOSTGROUP="${GEARMAN_HOSTGROUP:-gearman-test}"
GEARMAN_DEBUG="${GEARMAN_DEBUG:-0}"
CHECK_INTERVAL="${CHECK_INTERVAL:-5}"
WORK="${WORK:-/gearman-test}"
CAPTURE_DIR="${CAPTURE_DIR:-}"

case "$GEARMAN_CORE" in
  nagios)
    CORE_BIN="${CORE_BIN:-/usr/local/nagios/bin/nagios}"
    CORE_USER="${CORE_USER:-nagios}"
    NEB_MODULE="${NEB_MODULE:-/usr/local/nagios-mod-gearman/lib/nagios-mod-gearman/nagios-mod-gearman.o}"
    SEND_GEARMAN="${SEND_GEARMAN:-/usr/local/nagios-mod-gearman/bin/nagios-send-gearman}"
    GEARMAN_TOP="${GEARMAN_TOP:-/usr/local/nagios-mod-gearman/bin/nagios-gearman-top}"
    USER_KEY="nagios_user"
    GROUP_KEY="nagios_group"
    ;;
  naemon)
    CORE_BIN="${CORE_BIN:-/usr/bin/naemon}"
    CORE_USER="${CORE_USER:-naemon}"
    NEB_MODULE="${NEB_MODULE:-/usr/lib/mod_gearman/mod_gearman_naemon.o}"
    SEND_GEARMAN="${SEND_GEARMAN:-/usr/bin/send_gearman}"
    GEARMAN_TOP="${GEARMAN_TOP:-/usr/bin/gearman_top}"
    USER_KEY="naemon_user"
    GROUP_KEY="naemon_group"
    ;;
  *)
    echo "!! GEARMAN_CORE must be nagios or naemon, got '$GEARMAN_CORE'" >&2
    exit 2
    ;;
esac

# The ConSol packages have moved the module between /usr/lib/mod_gearman and
# /usr/lib/<multiarch>/mod_gearman over the years; fall back to a search so
# a package layout change fails with a clear message instead of a broker
# module load error.
if [ ! -f "$NEB_MODULE" ]; then
  found="$(find / -name "$(basename "$NEB_MODULE")" -type f 2>/dev/null | head -n1 || true)"
  if [ -n "$found" ]; then
    echo ">> NEB module not at $NEB_MODULE, using $found"
    NEB_MODULE="$found"
  else
    echo "!! NEB module $NEB_MODULE not found" >&2
    exit 2
  fi
fi
for bin in "$CORE_BIN" "$SEND_GEARMAN"; do
  if [ ! -x "$bin" ]; then
    echo "!! $bin missing or not executable" >&2
    exit 2
  fi
done

echo ">> core=$GEARMAN_CORE bin=$CORE_BIN neb=$NEB_MODULE key=<${#GEARMAN_KEY} bytes> encryption=$GEARMAN_ENCRYPTION hostgroup=$GEARMAN_HOSTGROUP"

# ---------------------------------------------------------------------------
# Directory layout. Everything the core writes lives under $WORK/var so the
# tests can `cat $WORK/var/status.dat` regardless of the distribution.
# ---------------------------------------------------------------------------
ETC="$WORK/etc"
VAR="$WORK/var"
rm -rf "$ETC" "$VAR"
mkdir -p "$ETC" "$VAR/rw" "$VAR/checkresults" "$VAR/archives" "$VAR/log"

# ---------------------------------------------------------------------------
# 1. gearmand
# ---------------------------------------------------------------------------
echo ">> Starting gearmand on $GEARMAND_LISTEN:$GEARMAN_PORT..."
gearmand --listen="$GEARMAND_LISTEN" --port="$GEARMAN_PORT" \
  --log-file="$VAR/gearmand.log" --pid-file="$VAR/gearmand.pid" --daemon
for _ in $(seq 1 50); do
  if gearadmin --port="$GEARMAN_PORT" --status >/dev/null 2>&1; then break; fi
  sleep 0.2
done
gearadmin --port="$GEARMAN_PORT" --server-version

# ---------------------------------------------------------------------------
# 2. Mod-Gearman NEB module config. Same keys in both flavours.
# ---------------------------------------------------------------------------
cat > "$ETC/module.conf" <<EOF
debug=$GEARMAN_DEBUG
logfile=$VAR/log/mod_gearman_neb.log
server=127.0.0.1:$GEARMAN_PORT
encryption=$GEARMAN_ENCRYPTION
key=$GEARMAN_KEY
# Only the test hostgroup goes through gearmand; the generic host/service
# queues stay off so nothing but hostgroup_$GEARMAN_HOSTGROUP is ever used.
hostgroups=$GEARMAN_HOSTGROUP
do_hostchecks=yes
hosts=no
services=no
eventhandler=no
notifications=no
result_workers=1
use_uniq_jobs=on
orphan_host_checks=yes
orphan_service_checks=yes
accept_clear_results=no
perfdata=no
EOF

# ---------------------------------------------------------------------------
# 3. Object config (identical for both cores).
# ---------------------------------------------------------------------------
cat > "$ETC/objects.cfg" <<EOF
define timeperiod {
  timeperiod_name  24x7
  alias            Always
  sunday           00:00-24:00
  monday           00:00-24:00
  tuesday          00:00-24:00
  wednesday        00:00-24:00
  thursday         00:00-24:00
  friday           00:00-24:00
  saturday         00:00-24:00
}

define command {
  command_name  nscp
  command_line  \$ARG1\$
}

define command {
  command_name  notify-nobody
  command_line  /bin/true
}

define contact {
  contact_name                   nobody
  alias                          Nobody
  host_notifications_enabled     0
  service_notifications_enabled  0
  host_notification_period       24x7
  service_notification_period    24x7
  host_notification_options      n
  service_notification_options   n
  host_notification_commands     notify-nobody
  service_notification_commands  notify-nobody
}

define hostgroup {
  hostgroup_name  $GEARMAN_HOSTGROUP
  alias           Hosts checked through gearmand
}

define host {
  host_name              nscp-test
  alias                  NSClient++ under test
  address                127.0.0.1
  hostgroups             $GEARMAN_HOSTGROUP
  check_command          nscp!check_always_ok check_ok message=host-is-up
  check_interval         $CHECK_INTERVAL
  retry_interval         $CHECK_INTERVAL
  max_check_attempts     1
  check_period           24x7
  contacts               nobody
  notification_interval  0
  notification_period    24x7
  notifications_enabled  0
}

define service {
  host_name              nscp-test
  service_description    helper
  check_command          nscp!check_ok message=hello
  check_interval         $CHECK_INTERVAL
  retry_interval         $CHECK_INTERVAL
  max_check_attempts     1
  check_period           24x7
  contacts               nobody
  notification_interval  0
  notification_period    24x7
  notifications_enabled  0
}

define service {
  host_name              nscp-test
  service_description    cpu
  check_command          nscp!check_cpu "warning=usage gt 101" "critical=usage gt 101"
  check_interval         $CHECK_INTERVAL
  retry_interval         $CHECK_INTERVAL
  max_check_attempts     1
  check_period           24x7
  contacts               nobody
  notification_interval  0
  notification_period    24x7
  notifications_enabled  0
}

define service {
  host_name              nscp-test
  service_description    slow
  check_command          nscp!check_slow
  check_interval         $CHECK_INTERVAL
  retry_interval         $CHECK_INTERVAL
  max_check_attempts     1
  check_period           24x7
  contacts               nobody
  notification_interval  0
  notification_period    24x7
  notifications_enabled  0
}
EOF

# ---------------------------------------------------------------------------
# 4. Core config. The two cores accept the same keys except the user/group
#    ones, which carry the product name.
# ---------------------------------------------------------------------------
cat > "$ETC/core.cfg" <<EOF
cfg_file=$ETC/objects.cfg
log_file=$VAR/log/core.log
log_archive_path=$VAR/archives
log_rotation_method=n
use_syslog=0
log_initial_states=1
object_cache_file=$VAR/objects.cache
precached_object_file=$VAR/objects.precache
status_file=$VAR/status.dat
status_update_interval=1
$USER_KEY=$CORE_USER
$GROUP_KEY=$CORE_USER
check_external_commands=1
command_file=$VAR/rw/core.cmd
query_socket=$VAR/rw/core.qh
lock_file=$VAR/core.pid
temp_file=$VAR/core.tmp
temp_path=$VAR
check_result_path=$VAR/checkresults
state_retention_file=$VAR/retention.dat
retain_state_information=0
use_retained_scheduling_info=0
# Every interval below is in seconds, and the first checks start at once.
interval_length=1
max_service_check_spread=1
max_host_check_spread=1
service_check_timeout=3
host_check_timeout=3
service_check_timeout_state=c
check_for_orphaned_services=1
check_for_orphaned_hosts=1
execute_service_checks=1
execute_host_checks=1
accept_passive_service_checks=1
accept_passive_host_checks=1
enable_notifications=0
enable_event_handlers=0
process_performance_data=0
illegal_macro_output_chars=\`~\$&|'"<>
debug_level=0
debug_file=$VAR/core.debug
daemon_dumps_core=0
event_broker_options=-1
broker_module=$NEB_MODULE config=$ETC/module.conf
EOF

chown -R "$CORE_USER:$CORE_USER" "$WORK"

# Naemon refuses to start as root and Nagios would drop privileges itself;
# run the core as its own user either way (setpriv rather than runuser: no
# PAM, no session, no TTY handling, see the Icinga entrypoint).
run_core() {
  if [ "$(id -u)" = "0" ]; then
    setpriv --reuid="$CORE_USER" --regid="$CORE_USER" --init-groups -- "$CORE_BIN" "$@"
  else
    "$CORE_BIN" "$@"
  fi
}

echo ">> Verifying configuration..."
run_core -v "$ETC/core.cfg" | tail -n 5

# ---------------------------------------------------------------------------
# The core takes a moment to load the NEB module and its result thread; keep
# it in the foreground when serving, in the background when capturing.
# ---------------------------------------------------------------------------
if [ -z "$CAPTURE_DIR" ]; then
  echo ">> Starting $GEARMAN_CORE in the foreground (status file: $VAR/status.dat)"
  if [ "$(id -u)" = "0" ]; then
    exec setpriv --reuid="$CORE_USER" --regid="$CORE_USER" --init-groups -- "$CORE_BIN" "$ETC/core.cfg"
  fi
  exec "$CORE_BIN" "$ETC/core.cfg"
fi

# ===========================================================================
# Capture mode.
# ===========================================================================
mkdir -p "$CAPTURE_DIR"
QUEUE="hostgroup_$GEARMAN_HOSTGROUP"

# Decrypt one base64 envelope the way common/gm_crypt.c does: base64, then
# AES-256-ECB with the key NUL-padded to 32 bytes and OpenSSL padding off,
# then drop the NUL padding. Plain mode (encryption=no) is base64 only.
decode_payload() {
  if [ "$GEARMAN_ENCRYPTION" = "yes" ]; then
    local hex
    hex="$(printf '%s' "$GEARMAN_KEY" | head -c 32 | od -An -tx1 -v | tr -d ' \n')"
    hex="$(printf '%-64s' "$hex" | tr ' ' 0)"
    base64 -d | openssl enc -d -aes-256-ecb -K "$hex" -nopad | tr -d '\0'
  else
    base64 -d
  fi
}

grab_one() { # grab_one <queue> <file> <timeout-seconds>
  timeout "$3" gearman -h 127.0.0.1 -p "$GEARMAN_PORT" -w -c 1 -f "$1" > "$2" || true
  [ -s "$2" ]
}

echo ">> Starting $GEARMAN_CORE in the background for capture..."
run_core "$ETC/core.cfg" &
CORE_PID=$!
trap 'kill $CORE_PID 2>/dev/null || true' EXIT

echo ">> Capturing jobs from queue $QUEUE..."
have_host=""
have_service=""
for n in $(seq 1 20); do
  if ! kill -0 "$CORE_PID" 2>/dev/null; then
    echo "!! $GEARMAN_CORE exited before a job was captured" >&2
    exit 1
  fi
  raw="$CAPTURE_DIR/.raw-$n.b64"
  if ! grab_one "$QUEUE" "$raw" 30; then
    echo "!! no job arrived on $QUEUE within 30s (attempt $n)" >&2
    continue
  fi
  text="$(decode_payload < "$raw")"
  type="$(printf '%s\n' "$text" | sed -n 's/^type=//p' | head -n1)"
  service="$(printf '%s\n' "$text" | sed -n 's/^service_description=//p' | head -n1)"
  echo "   got a $type job${service:+ for service $service} ($(wc -c < "$raw") bytes of base64)"
  # The service fixture is always the `helper` service, so the checked-in
  # payload does not depend on which service the core happened to schedule
  # first.
  case "$type" in
    host)    [ -z "$have_host" ]    && mv "$raw" "$CAPTURE_DIR/$GEARMAN_CORE-job-host.b64"    && have_host=1 ;;
    service) [ -z "$have_service" ] && [ "$service" = "helper" ] && mv "$raw" "$CAPTURE_DIR/$GEARMAN_CORE-job-service.b64" && have_service=1 ;;
  esac
  rm -f "$raw"
  [ -n "$have_host" ] && [ -n "$have_service" ] && break
done
if [ -z "$have_host" ] || [ -z "$have_service" ]; then
  echo "!! did not capture both a host and a service job" >&2
  exit 1
fi

# Results: send_gearman is the reference producer of result payloads (the
# same code path the worker uses). Send them to a side queue the NEB result
# thread is not listening on, so we can grab them raw. --active with fixed
# times gives a byte-stable payload for the unit tests.
echo ">> Capturing results via $SEND_GEARMAN..."
common=(--server="127.0.0.1:$GEARMAN_PORT" --encryption="$GEARMAN_ENCRYPTION" --key="$GEARMAN_KEY" --result_queue=capture_results)
"$SEND_GEARMAN" "${common[@]}" --host=nscp-test --service=helper --returncode=0 \
  --message="OK: hello|'time'=1ms;5;10"
grab_one capture_results "$CAPTURE_DIR/$GEARMAN_CORE-result-passive-service.b64" 10
"$SEND_GEARMAN" "${common[@]}" --host=nscp-test --returncode=1 \
  --message="WARNING: host result"
grab_one capture_results "$CAPTURE_DIR/$GEARMAN_CORE-result-passive-host.b64" 10
"$SEND_GEARMAN" "${common[@]}" --host=nscp-test --service=helper --returncode=2 --active \
  --starttime=1757930400 --finishtime=1757930401 --latency=0.5 \
  --message="CRITICAL: active result
second line|'load'=12%;80;90"
grab_one capture_results "$CAPTURE_DIR/$GEARMAN_CORE-result-active-service.b64" 10

{
  echo "core=$GEARMAN_CORE"
  echo "core: $("$CORE_BIN" --version 2>/dev/null | grep -m1 -i 'core' || true)"
  echo "tools: $("$SEND_GEARMAN" --version 2>&1 | grep -m1 -i 'version' || true)"
  echo "gearmand: $(gearadmin --port="$GEARMAN_PORT" --server-version 2>/dev/null)"
  echo "encryption=$GEARMAN_ENCRYPTION"
  echo "key=$GEARMAN_KEY"
  echo "hostgroup=$GEARMAN_HOSTGROUP"
} > "$CAPTURE_DIR/$GEARMAN_CORE-versions.txt"

for f in job-host job-service result-passive-service result-passive-host result-active-service; do
  p="$CAPTURE_DIR/$GEARMAN_CORE-$f.b64"
  if [ ! -s "$p" ]; then
    echo "!! missing capture $p" >&2
    exit 1
  fi
  echo ">> $p:"
  decode_payload < "$p" | sed 's/^/     /'
done
echo ">> Capture complete."
