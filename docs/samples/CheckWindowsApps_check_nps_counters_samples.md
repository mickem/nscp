#### Host without NPS

Captured on Windows without the role; the result is UNKNOWN.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_counters
NPS counters unavailable: NPS role is not installed (IAS service missing)
```

#### Select an installed counter object

Configuration examples for an English Windows NPS host:

```text
check_nps_counters
check_nps_counters "object=NPS Accounting Server" sample-ms=1000
```

Use the returned `counter` and `instance` names when setting a filter and
threshold. Counter names and available instances depend on the Windows version
and language. Select individual clients or `_Total` when available, avoiding
double-counting both in a graph.
