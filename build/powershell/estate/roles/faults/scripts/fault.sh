#!/bin/sh
# Shipped by the `faults` role bundle and run by the agent (as the nsclient
# user) as `sh scripts/fault.sh <fault>`: switches one misbehaviour of the
# Acme Agent the role installed (install-ubuntu.sh) on, or every one off.
#
#   cpu      one acme-indexer per core, spinning
#   memory   acme-cache holding all but ~8% of the available memory
#   disk     /var/lib/acme/cache/index.bin, filling / to ~93%
#   service  acme-sync crash-loops until `clear`
#   clear    all of the above undone
#
# A started fault runs in its own session with its output on /dev/null, so
# the agent gets its answer at once and does not wait on it; each one also
# ends by itself after an hour. Prints a Nagios line: OK when the fault is on
# (or off, for clear), UNKNOWN when it could not be.
bin=/opt/acme/bin
state=/var/lib/acme
pids="$state/faults.pids"

fail() {
  echo "UNKNOWN: $*"
  exit 3
}

# Start "$@" detached, in a session of its own whose id is recorded, so clear
# can stop the whole group (a pipeline included).
detach() {
  setsid sh -c "$*" </dev/null >/dev/null 2>&1 &
  echo $! >>"$pids"
}

[ -x "$bin/acme-indexer" ] || fail "the Acme Agent is not installed ($bin)"
[ -w "$state" ] || fail "$state is not writable by $(id -un)"

case "${1:-}" in
  cpu)
    cores=$(nproc)
    i=0
    while [ "$i" -lt "$cores" ]; do
      detach "exec nice -n 10 timeout 3600 $bin/acme-indexer >/dev/null"
      i=$((i + 1))
    done
    echo "OK: acme-indexer started on $cores core(s)"
    ;;
  memory)
    total_kb=$(awk '/^MemTotal:/ {print $2}' /proc/meminfo)
    avail_kb=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)
    bytes=$(((avail_kb - total_kb * 8 / 100) * 1024))
    [ "$bytes" -gt 0 ] || fail "only ${avail_kb} kB available of ${total_kb} kB: nothing left to take"
    # tail keeps everything after the last newline, and zeros have none; the
    # sleep keeps its input open, so it holds them instead of finishing.
    detach "{ head -c $bytes /dev/zero; exec sleep 3600; } | $bin/acme-cache"
    echo "OK: acme-cache is taking $((bytes / 1048576)) MiB"
    ;;
  disk)
    set -- $(df -P -B1 "$state/cache" | awk 'NR == 2 {print $2, $3, $4}')
    size=$1 used=$2 avail=$3
    want=$((size * 93 / 100 - used))
    cap=$((avail - 268435456))
    [ "$want" -gt "$cap" ] && want=$cap
    [ "$want" -gt 0 ] || fail "the disk is already at $((used * 100 / size))%"
    fallocate -l "$want" "$state/cache/index.bin" || fail "fallocate of $want bytes failed"
    echo "OK: wrote $((want / 1048576)) MiB to $state/cache/index.bin"
    ;;
  service)
    : >"$state/sync.fail" || fail "could not create $state/sync.fail"
    echo "OK: acme-sync will crash until cleared"
    ;;
  clear)
    if [ -f "$pids" ]; then
      while read -r p; do
        kill -TERM -- "-$p" 2>/dev/null
      done <"$pids"
      rm -f "$pids"
    fi
    pkill -x acme-indexer 2>/dev/null
    pkill -x acme-cache 2>/dev/null
    rm -f "$state/cache/index.bin" "$state/sync.fail"
    echo "OK: every Acme fault is cleared"
    ;;
  *)
    fail "usage: fault.sh cpu|memory|disk|service|clear"
    ;;
esac
exit 0
