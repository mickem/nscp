#### About `check_hostname` (Windows)

`check_hostname` reports host identity — hostname, FQDN, DNS domain and
domain-join state — and detects the name drift that silently breaks Kerberos
auth, certificate validation and monitoring host-matching. It reads
`GetComputerNameEx` (NetBIOS name, DNS hostname, DNS suffix, FQDN) and
`NetGetJoinInformation` (joined domain or workgroup); no WMI involved.

The useful alerts are **pinned expectations**:

- **"Is this box still on the domain?"** — `crit=join != 'domain'` or
  `crit=domain != 'corp.example.com'` catches domain-join / workgroup drift.
- **"Is the name coherent?"** — `warn=fqdn_consistent = 0` (the FQDN no longer
  equals `dns_hostname.domain`: DNS-suffix drift) and
  `warn=netbios_matches_dns = 0` (NetBIOS name diverged from the DNS hostname
  after a rename or re-image).
- **"Is this the host I think it is?"** — `crit=hostname != 'WEB01'` on
  cloned/re-imaged machines.

There are no default thresholds — whether `workgroup` is wrong is site policy —
and no perf data (there is no meaningful number here). A host with no DNS
suffix reports `fqdn == hostname` as consistent, not as drift, and the NetBIOS
comparison tolerates the 15-character truncation of longer DNS names.

On Linux the same command is provided by the unix CheckSystem module with the
shared keywords (`hostname`, `fqdn`, `domain`, `fqdn_consistent`); `join` /
`join_name` have no clean Linux equivalent and are absent there. See also
CheckSecurity's `check_nla` for the runtime side of the same question — which
network profile (domain/private/public) the host is currently on.
