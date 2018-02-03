#!/bin/sh
### BEGIN INIT INFO
# Provides:          NSClient++
# Required-Start:    $local_fs $network $named $time $syslog
# Required-Stop:     $local_fs $network $named $time $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Description:       Daemon for starting and stopping nscp (nsclient++)
### END INIT INFO

SCRIPT=/usr/sbin/nscp
RUNAS=nsclient
NAME=nsclient

PIDFILE=/var/run/$NAME.pid
LOGFILE=/var/log/$NAME/$NAME.log

start() {
  if [ -f $PIDFILE ] && kill -0 $(cat $PIDFILE); then
    echo 'Service already running' >&2
    return 1
  fi
  echo 'Starting service...' >&2
  local CMD="$SCRIPT service --run &> \"$LOGFILE\" & echo \$!"
  su -s /bin/bash -c "$CMD" $RUNAS > "$PIDFILE"
  echo 'Service started' >&2
}

stop() {
  if [ ! -f "$PIDFILE" ] || ! kill -0 $(cat "$PIDFILE"); then
    echo 'Service not running' >&2
    return 1
  fi
  echo 'Stopping service...' >&2
  kill -15 $(cat "$PIDFILE") && rm -f "$PIDFILE"
  echo 'Service stopped' >&2
}

status() {
        printf "%-50s" "Checking $NAME..."
    if [ -f $PIDFILE ]; then
        PID=$(cat $PIDFILE)
            if [ -z "$(ps axf | grep ${PID} | grep -v grep)" ]; then
                printf "%s\n" "The process appears to be dead but pidfile still exists"
            else    
                echo "Running, the PID is $PID"
            fi
    else
        printf "%s\n" "Service not running"
    fi
}


case "$1" in
  start)
    start
    ;;
  stop)
    stop
    ;;
  status)
    status
    ;;
  restart)
    stop
    start
    ;;
  *)
    echo "Usage: $0 {start|stop|status|restart}"
esac
