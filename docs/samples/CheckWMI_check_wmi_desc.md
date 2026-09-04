#### About `check_wmi`

`check_wmi` runs an arbitrary WQL query and turns each returned row into a
filter record. It is the escape hatch for everything the purpose-built checks do
not cover: if the data is in WMI, this exposes it to the same
`filter=` / `warning=` / `critical=` / `top-syntax=` machinery as every other
check.

##### Keywords are the query's own columns

Unlike every other check, the keyword vocabulary is **not fixed** — it is
derived from the columns your `query=` selects. A query of
`SELECT Name, FreeSpace FROM Win32_LogicalDisk` gives you `Name` and
`FreeSpace` as keywords, and each is also emitted as performance data. Because
of that, the "Filter keywords" table below lists only `line` (the whole row,
rendered as a comma-separated string, which the default `detail-syntax` uses);
everything else depends on the query.

This also means `SELECT *` is usually the wrong thing to write: it produces a
keyword and a performance-data series per column of the class, most of which you
do not want graphed. Select the columns you intend to use.

##### Remote hosts

`target=` names a host to query instead of the local machine, resolved through
the module's configured targets when one matches and treated as a hostname
otherwise; `user=` and `password=` supply alternate credentials.
`namespace=` binds to a WMI root other than the default `root\cimv2` —
`root\wmi` for driver/ACPI classes, `root\Microsoft\Windows\Storage` for Storage
Spaces, and so on.

Remote WMI needs DCOM/RPC reachable through the firewall and an account with
remote-WMI rights on the target. Where the data is available locally, running
the check on the monitored host is both faster and simpler to secure.

##### Caveats

There are no default thresholds, and the default `empty-state` is `ignored`, so
a query that matches nothing returns OK. If "no rows" is itself the alarm
condition — a service class that should always have an instance — set
`empty-state=critical` explicitly.

A WMI query is comparatively expensive and can block for seconds on a busy or
unhealthy host; keep queries narrow and consider wrapping slow ones in
[`check_timeout`](../check/CheckHelpers.md#check_timeout).
