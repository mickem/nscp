#### Host without NPS

Captured on Windows without the role; the result is UNKNOWN.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_accounting
NPS accounting data unavailable: NPS role is not installed (IAS service missing)
```

#### Accounting discard and log checks

Configuration examples; choose the current accounting filename after rotation:

```text
check_nps_accounting window=300
check_nps_accounting log-file=C:\Windows\System32\LogFiles\IN260928.log
check_nps_accounting log-file=C:\Windows\System32\LogFiles\IN260928.log require-traffic=true max-age=600
```

The first command evaluates discarded requests only. The second also checks the
specified file exists and reports its age/size. The third treats empty or stale
output as a problem because accounting traffic is expected.
