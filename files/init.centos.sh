#!/bin/bash
### BEGIN INIT INFO
# Provides: nsclient
# Required-Start: $local_fs $network $syslog
# Required-Stop: $local_fs $network $syslog
# Default-Start: 2 3 4 5
# Default-Stop: 0 1 6
# Short-Description: start and stop postfix
# Description: NSClient++ is a monitoring agent, which means it will help
#              with monitoring.
### END INIT INFO
SCRIPT=/usr/sbin/nscp
RUNAS=nsclient
NAME=nsclient

PIDFILE=/usr/share/nsclient/nscp.pid
LOGFILE=/var/log/$NAME/$NAME.log

# Source function library.
. /etc/init.d/functions

start() {
	echo -n "Starting $NAME: "
	touch $PIDFILE
	chown nsclient $PIDFILE
	daemon --user $RUNAS --pidfile "$PIDFILE" "/usr/sbin/nscp service --run --pid $PIDFILE 2>&1 > $LOGFILE &"
	RESULT=$?
	echo
	return $RESULT
}

stop() {
	echo -n "Shutting down $NAME: "
	killproc -p $PIDFILE
	echo
	return 0
}

case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
	status)
		status -p $PIDFILE $NAME
		;;
	restart)
		stop
		start
		;;
	reload)
		restart
		;;
	condrestart)
		restart
		;;
	*)
		echo "Usage: $NAME {start|stop|status|reload|restart"
		exit 1
		;;
esac
exit $?

