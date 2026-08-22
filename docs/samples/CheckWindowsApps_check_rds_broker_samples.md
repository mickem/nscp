**List the Connection Broker counters (one record per counter, whatever this broker exposes):**

```
check_rds_broker show-all
OK: RDCB Connection Requests Failed = 0, RDCB Connection Requests Pending = 1, RDCB Connection Requests Total = 4211|'RDCB Connection Requests Failed'=0;0;0 'RDCB Connection Requests Pending'=1;0;0 'RDCB Connection Requests Total'=4211;0;0
```

**Alert on failed or piling-up connection requests:**

```
check_rds_broker "warning=counter like 'Pending' and value > 20" "critical=counter like 'Failed' and value > 0"
CRITICAL: RDCB Connection Requests Failed = 17|'RDCB Connection Requests Failed'=17;0;0 ...
```

**Sample rate counters over a second:**

```
check_rds_broker averages=true "filter=counter like 'Requests'"
OK: RDCB Connection Requests Total = 4211|'RDCB Connection Requests Total'=4211;0;0 ...
```

**On a host that is not a Connection Broker the check reports UNKNOWN with a clear message:**

```
check_rds_broker
Connection Broker counters (Remote Desktop Connection Broker Counterset) not available - is this host an RD Connection Broker? (Failed to enumerate object: Remote Desktop Connection Broker Counterset)
```
