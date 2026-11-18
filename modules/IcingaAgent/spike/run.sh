#!/bin/sh
# Start the master, issue a ticket for the agent, start the agent, then show
# what the master sees. Needs NSCP_BUILD_DIR (a Linux build tree with nscp,
# IcingaAgent, CheckSystem, CheckHelpers, CheckDisk and CommandClient built).
set -e
cd "$(dirname "$0")"
docker compose up -d icinga-master
echo "Waiting for the master..."
until docker compose exec -T icinga-master icinga2 pki ticket --cn nscp-agent >/dev/null 2>&1; do sleep 2; done
ICINGA_TICKET=$(docker compose exec -T icinga-master icinga2 pki ticket --cn nscp-agent | tr -d '\r')
export ICINGA_TICKET
docker compose up -d --build nscp-agent
echo "Waiting for check results..."
sleep 45
curl -sk -u spike:spike \
  'https://127.0.0.1:5665/v1/objects/services?filter=host.name==%22nscp-agent%22&attrs=name&attrs=state&attrs=last_check_result' |
  python3 -c '
import json, sys
for r in json.load(sys.stdin)["results"]:
    a = r["attrs"]; cr = a["last_check_result"] or {}
    print("%-12s %s  %s" % (a["name"], ["OK", "WARNING", "CRITICAL", "UNKNOWN"][int(a["state"])], (cr.get("output") or "").splitlines()[0][:100]))
    print("%-12s perfdata: %s" % ("", cr.get("performance_data")))
'
