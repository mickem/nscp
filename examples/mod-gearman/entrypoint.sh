#!/bin/bash
# Three processes, one container: gearmand, Thruk and Naemon. A real
# installation would run them as three services (and Thruk behind apache);
# this is a lab, and one `docker compose up` is worth more here than
# correctness of deployment.
set -eu

GEARMAN_PORT="${GEARMAN_PORT:-4730}"
THRUK_PORT="${THRUK_PORT:-8080}"
NEB_MODULE="${NEB_MODULE:-/opt/mod_gearman/mod_gearman_naemon.o}"
MODULE_CONF="${MODULE_CONF:-/etc/mod-gearman/module.conf}"

# Naemon refuses to run as root - verification included - so everything it
# touches goes through its own user.
as_naemon() {
  setpriv --reuid=naemon --regid=naemon --init-groups -- "$@"
}

echo ">> gearmand on 0.0.0.0:$GEARMAN_PORT"
gearmand --listen=0.0.0.0 --port="$GEARMAN_PORT" \
    --log-file=/var/log/gearmand.log --pid-file=/var/run/gearmand.pid --daemon
for _ in $(seq 1 50); do
  gearadmin --port="$GEARMAN_PORT" --status >/dev/null 2>&1 && break
  sleep 0.2
done
gearadmin --port="$GEARMAN_PORT" --server-version

# The broker_module line is the only thing this lab adds to the core's own
# configuration; everything else about Mod-Gearman is in module.conf, which
# is bind-mounted from conf/ so it can be edited without a rebuild.
echo ">> Loading the Mod-Gearman NEB module from $NEB_MODULE"
cat > /etc/naemon/module-conf.d/mod_gearman.cfg <<CFG
broker_module=$NEB_MODULE config=$MODULE_CONF
CFG

mkdir -p /var/cache/naemon /var/lib/naemon /var/log/naemon
chown -R naemon:naemon /var/cache/naemon /var/lib/naemon /var/log/naemon

echo ">> Verifying the Naemon configuration"
if ! as_naemon naemon -v /etc/naemon/naemon.cfg > /tmp/verify.log 2>&1; then
  tail -n 30 /tmp/verify.log >&2
  echo "!! The configuration in conf/ does not parse - see above." >&2
  exit 2
fi
grep -E "Total (Warnings|Errors)" /tmp/verify.log || true

# Thruk is meant to run behind apache with fcgid; Starman serves the same
# application from a .psgi, which is one moving part fewer. It has to be a
# forking server: a browser opens half a dozen connections to a page, and
# plackup's single-process default server answers them one at a time, which
# looks exactly like the UI hanging.
echo ">> Thruk on 0.0.0.0:$THRUK_PORT"
export THRUK_CONFIG=/etc/thruk
export PERL5LIB=/usr/share/thruk/lib
(
  cd /usr/share/thruk
  exec plackup -s Starman --workers 5 --port "$THRUK_PORT" --host 0.0.0.0 \
      /opt/thruk.psgi >/var/log/thruk.log 2>&1
) &
THRUK_PID=$!
trap 'kill $THRUK_PID 2>/dev/null || true' EXIT

echo ">> Naemon (log: docker compose logs, status: the web UI on $THRUK_PORT)"
# (exec cannot run a shell function, so setpriv is spelled out here.)
exec setpriv --reuid=naemon --regid=naemon --init-groups -- naemon /etc/naemon/naemon.cfg
