#### Host without NPS

Captured with the one-shot client on Windows without the NPS role. The command
returns UNKNOWN; the client prints the check message without adding a status word.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_auth
NPS authentication data unavailable: NPS role is not installed (IAS service missing)
```

#### Aggregate and per-client policy

Configuration examples for an NPS host with auditing enabled:

```text
check_nps_auth window=300 min-requests=20
check_nps_auth window=300 group-by=client min-requests=50
check_nps_auth window=600 group-by=policy "filter=group = 'Corporate WiFi'"
check_nps_auth window=300 require-traffic=true
```

Custom percentage thresholds should preserve the minimum-volume guard and the
independent discard alert:

```text
check_nps_auth "warning=decisions >= min_requests and reject_pct > 15" "critical=discarded > 0 or (decisions >= min_requests and reject_pct > 30)"
```

Use the separate IAS service check and certificate check alongside the traffic
summary. A quiet authentication window alone does not establish service health.
