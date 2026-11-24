**Default check:**

One share on a Scale-Out File Server is slow and the client is waiting for
credits; the share on `fs01` is healthy. `IPC$` (named pipes to the domain
controller) is left out by the default filter.

```
check_smb_client
WARNING: \\fs01\data: read 4.1ms, write 6.2ms, 0 credit stalls/s, queue 0, \\sofs01\VMs: read 31.2ms, write 18.7ms, 2 credit stalls/s, queue 7|'fs01/data_credit_stalls'=0;0 'fs01/data_latency'=6.2ms;25;50 'fs01/data_queue_length'=0 'fs01/data_read_latency'=4.1ms 'fs01/data_write_latency'=6.2ms 'sofs01/VMs_credit_stalls'=2;0 'sofs01/VMs_latency'=31.2ms;25;50 'sofs01/VMs_queue_length'=7 'sofs01/VMs_read_latency'=31.2ms 'sofs01/VMs_write_latency'=18.7ms
```

**Short thresholds:**

`-w` and `-c` bound `latency` (the slower of read and write, in
milliseconds); the warning keeps its credit-stall condition.

```
check_smb_client -w 10 -c 30
CRITICAL: \\fs01\data: read 4.1ms, write 6.2ms, 0 credit stalls/s, queue 0, \\sofs01\VMs: read 31.2ms, write 18.7ms, 2 credit stalls/s, queue 7|'fs01/data_credit_stalls'=0;0 'fs01/data_latency'=6.2ms;10;30 ...
```

**Credit stalls on one file server only:**

```
check_smb_client "filter=server = 'sofs01'" "warning=credit_stalls > 0" "critical=credit_stalls > 10" "detail-syntax=${share}: ${credit_stalls} stalls/s, ${queue_length} queued"
WARNING: VMs: 2 stalls/s, 7 queued|'sofs01/VMs_credit_stalls'=2;0;10 'sofs01/VMs_queue_length'=7 'sofs01/VMs_read_latency'=31.2ms 'sofs01/VMs_write_latency'=18.7ms
```

**A host that must always have its share open:**

SMB sessions close when they go idle, so by default no open share is OK. An
application server that should never be without its data share can make the
empty state an alert:

```
check_smb_client "filter=server = 'fs01' and share = 'data'" empty-state=critical
CRITICAL: No SMB client shares in use|
```
