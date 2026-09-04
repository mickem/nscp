#### About `check_hostname`

`check_hostname` reports host identity — hostname, FQDN and DNS domain (plus
domain-join state on Windows) — and detects the name drift that silently breaks
Kerberos auth, certificate validation and monitoring host-matching.

The shared keywords (`hostname`, `fqdn`, `domain`, `fqdn_consistent`) carry the
same meaning on both platforms. The check returns a single aggregate row, has no
default thresholds — whether `workgroup` is wrong is site policy — and emits no
performance data (there is no meaningful number here). Comparisons are
case-insensitive, since DNS is case-insensitive and case differences are not
drift.

##### Windows

Reads `GetComputerNameEx` (NetBIOS name, DNS hostname, DNS suffix, FQDN) and
`NetGetJoinInformation` (joined domain or workgroup); no WMI involved. Two extra
keywords are available here: `join` / `join_name` (Active Directory membership)
and `netbios_matches_dns`.

The useful alerts are **pinned expectations**:

- **"Is this box still on the domain?"** — `crit=join != 'domain'` or
  `crit=domain != 'corp.example.com'` catches domain-join / workgroup drift.
- **"Is the name coherent?"** — `warn=fqdn_consistent = 0` (the FQDN no longer
  equals `dns_hostname.domain`: DNS-suffix drift) and
  `warn=netbios_matches_dns = 0` (NetBIOS name diverged from the DNS hostname
  after a rename or re-image).
- **"Is this the host I think it is?"** — `crit=hostname != 'WEB01'` on
  cloned/re-imaged machines.

A host with no DNS suffix reports `fqdn == hostname` as consistent, not as
drift, and the NetBIOS comparison tolerates the 15-character truncation of
longer DNS names.

##### Linux

Reads `gethostname()` and canonicalises it with `getaddrinfo(AI_CANONNAME)`;
when the host cannot be resolved (containers, hosts without DNS) the FQDN falls
back to the bare hostname, which is treated as consistent rather than as drift.
`join` / `join_name` / `netbios_matches_dns` have no clean Linux equivalent and
are absent here.

The useful alerts are **pinned expectations**:

- **"Is this the host I think it is?"** — `crit=hostname != 'web01'`,
  `crit=domain != 'corp.example.com'`.
- **"Is the name coherent?"** — `warn=fqdn_consistent = 0` flags the resolver
  canonicalising this host under a *different* name (stale `/etc/hosts`
  entries, CNAME chains, re-imaged boxes keeping an old DNS record).

##### See also

CheckSecurity's `check_nla` covers the runtime side of the same question — which
network profile (domain/private/public) the host is currently on.
