#### About `check_ad_replication`

`check_ad_replication` reads the inbound replication state of a domain
controller straight from the directory service (`DsReplicaGetInfo`, the same
source `repadmin /showrepl` uses). Each inbound replication link — a (naming
context, source DC) pair — becomes one row: when it last attempted and last
managed to sync, and how many attempts in a row have failed.

Replication failures are the classic silent AD killer: a DC that has not
replicated for longer than the tombstone lifetime (typically 60–180 days) is
permanently orphaned and must be rebuilt. This check alerts long before that.

Keywords (one row per inbound replication link):

| Keyword                | Description                                                       |
|------------------------|-------------------------------------------------------------------|
| `naming_context`       | The replicated partition DN (e.g. `DC=example,DC=com`)            |
| `source`               | The source domain controller of the link                          |
| `source_dsa`           | Full DN of the source directory service agent                     |
| `source_address`       | Transport address of the source (GUID-based DNS name)             |
| `last_attempt`         | When a sync was last attempted (date)                             |
| `last_success`         | When a sync last succeeded (date; epoch 0 = never)                |
| `consecutive_failures` | Consecutive failed sync attempts (perf data)                      |
| `last_error`           | Win32 result of the last sync attempt (0 = success)               |
| `last_error_message`   | Human readable message for `last_error` (empty when ok)           |
| `failed`               | True when the last sync attempt failed                            |

Defaults: **WARNING** when `consecutive_failures > 0`, **CRITICAL** when
`consecutive_failures > 4 or last_success < -24h`. A link that has *never*
synced trips the 24-hour rule by design.

Options: `server=<dc>` checks another domain controller (default: the local
machine — replication state is per-DC, so run the check on every DC).

**Not-a-DC contract:** on a host that does not run the directory service the
check returns **UNKNOWN** with a "Not a domain controller" message rather than
a hard error, so it is safe to deploy fleet-wide. A single-DC domain (no
replication partners) returns **OK** with an explanatory empty-state message.
