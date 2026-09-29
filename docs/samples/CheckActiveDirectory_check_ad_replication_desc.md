#### About `check_ad_replication`

`check_ad_replication` reads the inbound replication state of a domain
controller straight from the directory service (`DsReplicaGetInfo`, the same
source `repadmin /showrepl` uses). Each inbound replication link — a (naming
context, source DC) pair — becomes one row: when it last attempted and last
managed to sync, and how many attempts in a row have failed.

Replication failures are the classic silent AD killer: a DC that has not
replicated for longer than the tombstone lifetime (typically 60–180 days) is
permanently orphaned and must be rebuilt. This check alerts long before that.

Defaults: **WARNING** when `consecutive_failures > 0`, **CRITICAL** when
`consecutive_failures > 4 or last_success < -24h`. A link that has *never*
synced trips the 24-hour rule by design.

Options: `server=<dc>` checks another domain controller (default: the local
machine — replication state is per-DC, so run the check on every DC).
`timeout=<ms>` (default 5000) bounds the whole read, the local machine
included.

**Not-a-DC contract:** on a host that is not a domain controller, the check
returns **UNKNOWN** with a "Not a domain controller" message rather than a hard
error, so it is safe to deploy fleet-wide. The machine role is what decides
this (`DsRoleGetPrimaryDomainInformation`), not the bind failure itself: a real
domain controller that fails to answer — stopped NTDS, access denied, RPC
unavailable — is reported as a plain failure, because that is the outage this
check exists to surface. A single-DC domain (no replication partners) returns
**OK** with an explanatory empty-state message.

**Timeout:** none of the directory service calls take a timeout of their own,
so the check runs the read on a worker thread and stops waiting for it when
`timeout=` runs out, reporting **UNKNOWN** ("No answer from the directory
service on dc02 within 5000ms"). That covers the case a port check misses: a
firewall that lets the RPC endpoint mapper (TCP 135) through but drops the
dynamic RPC port the directory service answers on, where the bind would
otherwise block for as long as RPC keeps retrying. A remote `server=` is still
probed on port 135 first, so a host that is down or blocked outright reports
exactly that. Windows cannot cancel the blocked call, so the worker is left to
finish on its own; until it has, further runs against the same server report
that the previous read has not returned instead of starting another thread.
