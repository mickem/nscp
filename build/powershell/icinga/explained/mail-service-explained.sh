#!/bin/bash
# Mails a service notification. For problems, asks the fleet server to
# explain the error first. The mail goes out explained or not.
set -u

. /etc/icinga2/fleet-explain.env
export FLEET_URL FLEET_API_KEY

explanation=""
if [ "$NOTIFICATIONTYPE" = "PROBLEM" ]; then
    # Optional head start for the agent's failure report (see Timing).
    sleep "${EXPLAIN_SETTLE:-0}"

    # The error goes in on stdin, so odd characters cannot be read as options.
    if out=$(printf '%s' "$SERVICEOUTPUT" | head -c 4000 |
             timeout 150 /usr/local/bin/check_nsclient fleet --timeout-s 120 \
                 explain --host "$FLEET_HOST" --check "$SERVICENAME" 2>&1); then
        explanation="$out"
    else
        explanation="No explanation available: $out"
    fi
fi

{
    echo "When:   $LONGDATETIME"
    echo "Output: $SERVICEOUTPUT"
    if [ -n "$explanation" ]; then
        echo
        echo "---- Why (model-written, from fleet evidence) ----"
        echo "$explanation"
    fi
} | mail -s "[$NOTIFICATIONTYPE] $SERVICENAME on $HOSTNAME is $SERVICESTATE" "$USEREMAIL"
