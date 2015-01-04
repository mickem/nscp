#!/bin/bash
#
#       /etc/rc.d/init.d/nscp
#
#       Daemon for starting and stopping nscp (nsclient++)
#       
#
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
		status $NAME
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

