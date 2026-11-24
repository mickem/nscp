The samples below read this file (`updates.csv`):

```
Hostname,WaitingUpdates,RebootRequired
web01,0,False
web02,7,True
db01,23,False
```

**Count the rows that match a filter (row-count mode):**

Alert when any host has updates waiting. Without a `detail-syntax` each
matching row is shown as its declared columns:

```
check_csv file=C:\exports\updates.csv header=true "columns=Hostname TEXT, WaitingUpdates INTEGER, RebootRequired TEXT" "filter=WaitingUpdates > 0" "critical=count > 0"
CRITICAL: Hostname=web02 WaitingUpdates=7 RebootRequired=True, Hostname=db01 WaitingUpdates=23 RebootRequired=False|'count'=2;;0
```

With a friendlier message, warning on one host and critical on more:

```
check_csv file=C:\exports\updates.csv header=true "columns=Hostname TEXT, WaitingUpdates INTEGER, RebootRequired TEXT" "filter=WaitingUpdates > 0" "warning=count > 0" "critical=count > 1" "detail-syntax=%(Hostname) (%(WaitingUpdates))" "top-syntax=%(status): %(count) hosts need updates: %(list)"
CRITICAL: 2 hosts need updates: web02 (7), db01 (23)|'count'=2;0;1
```

Nothing matching is OK:

```
check_csv file=C:\exports\updates.csv header=true "columns=Hostname TEXT, WaitingUpdates INTEGER" "filter=WaitingUpdates > 100" "critical=count > 0"
OK: No rows matched the filter|'count'=0;;0
```

**Threshold a single value (single-value mode):**

Pick one row with `filter=` and put the thresholds on the column. `perf-syntax`
names the performance data after the row:

```
check_csv file=C:\exports\updates.csv header=true "columns=Hostname TEXT, WaitingUpdates INTEGER" "filter=Hostname = 'db01'" "warning=WaitingUpdates > 10" "critical=WaitingUpdates > 50" "detail-syntax=%(Hostname): %(WaitingUpdates) updates waiting" "perf-syntax=%(Hostname)"
WARNING: db01: 23 updates waiting|'db01_WaitingUpdates'=23;10;50
```

When the row may be missing, say what that means with `empty-state`:

```
check_csv file=C:\exports\updates.csv header=true "filter=Hostname = 'web99'" empty-state=unknown
UNKNOWN: No rows matched the filter
```

**Take the column names from the header row:**

Without `columns=` every header becomes a TEXT column, which is enough for
string comparisons:

```
check_csv file=C:\exports\updates.csv header=true "critical=RebootRequired = 'True'" "detail-syntax=%(Hostname) needs a reboot"
CRITICAL: web02 needs a reboot
```

**Read a Windows PowerShell export:**

`Export-Csv -Encoding Unicode` on Windows PowerShell 5 writes UTF-16LE with a
byte-order mark (detected by the default `encoding=auto`) and starts with a
`#TYPE` line, which `skip-lines=1` drops:

```
check_csv file=C:\exports\services.csv skip-lines=1 header=true "critical=Status != 'Running'" "detail-syntax=%(Name) is %(Status)"
CRITICAL: wuauserv is Stopped
```

**Read a CSV from a share (Windows):**

A UNC path is read like any other path, with the agent's own credentials:

```
check_csv "file=\\fileserver\reports\updates.csv" header=true "columns=Hostname, WaitingUpdates INTEGER" "critical=WaitingUpdates > 20"
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_csv --argument "file=C:\exports\updates.csv" --argument "header=true" --argument "columns=Hostname, WaitingUpdates INTEGER" --argument "filter=WaitingUpdates > 0" --argument "critical=count > 0"
CRITICAL: Hostname=web02 WaitingUpdates=7, Hostname=db01 WaitingUpdates=23|'count'=2;;0
```
