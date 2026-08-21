#### About `check_hostname`

`check_hostname` reports host identity — the configured hostname, the
resolver's canonical FQDN and the derived DNS domain — and detects the name
drift that silently breaks auth and monitoring host-matching. It reads
`gethostname()` and canonicalises it with `getaddrinfo(AI_CANONNAME)`; when the
host cannot be resolved (containers, hosts without DNS) the FQDN falls back to
the bare hostname, which is treated as consistent rather than as drift.

It is the unix counterpart to the Windows `check_hostname`; the shared keywords
(`hostname`, `fqdn`, `domain`, `fqdn_consistent`) carry the same meaning on
both platforms. The Windows-only `join`/`join_name` (Active Directory
membership) have no clean Linux equivalent and are absent here.

The useful alerts are **pinned expectations**:

- **"Is this the host I think it is?"** — `crit=hostname != 'web01'`,
  `crit=domain != 'corp.example.com'`.
- **"Is the name coherent?"** — `warn=fqdn_consistent = 0` flags the resolver
  canonicalising this host under a *different* name (stale `/etc/hosts`
  entries, CNAME chains, re-imaged boxes keeping an old DNS record).

The check returns a single aggregate row. There are no default thresholds and no perf data. Comparisons are
case-insensitive (DNS is case-insensitive; case differences are not drift).
