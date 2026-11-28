#### About `check_dhcp_scopes`

`check_dhcp_scopes` reads every IPv4 scope of the DHCP server on this host
through the DHCP Server Management API — the same source the DHCP console and
`Get-DhcpServerv4ScopeStatistics` use: `DhcpEnumSubnets` lists the scopes,
`DhcpGetSubnetInfo` gives each one's mask, name and state, and `DhcpGetMibInfo`
gives the addresses in use, free and offered. Each scope becomes one row.

A scope that runs out of addresses takes every client on its subnet offline as
their leases expire, and nothing else on the server looks wrong while it
happens: the service is running, the counters are quiet, it simply answers
nobody. This check is the early warning.

Defaults: **WARNING** when `used_pct > 80` (or the scope has no statistics,
below), **CRITICAL** when `used_pct > 95`,
over the enabled scopes only (`filter=enabled = 1`) — a disabled scope leases
nothing and cannot run out. `-w`/`-c` bound `used_pct` (`w=70 c=90`). On a large
scope a percentage can leave a lot of room; threshold on the free addresses
instead (`warning=free < 50`) when a fixed reserve matters more.

`addresses` is what the scope can actually lease: its range minus its
exclusions, which is `in_use + free`. A scope whose whole range is excluded has
0 addresses and reads 0% used; it never alerts. Reservations count as
`in_use` once the reserved client has taken its lease.

**No-DHCP-server contract:** on a host without the DHCP Server role (or with
the service stopped) the check returns **UNKNOWN** with "DHCP Server service is
not running or not installed on this host". `dhcpsapi.dll` is delay-loaded,
so the other `CheckWindowsApps` checks never need it; should it be missing, the
check returns **UNKNOWN** saying so rather than failing the module.

**Nothing matched:** when no scope passes the filter the check is **UNKNOWN**
with "No DHCP scopes matched the filter (N on the server)". With the default
filter that means the server has no enabled scope; with your own filter it
means the filter matched nothing; N says how many scopes the server has in
all, so "0 on the server" is a server with no scopes. A check for exhaustion on
a server that leases nothing is pointed at the wrong host; pass
`empty-state=ok` where that is expected.

**Scopes without statistics:** the scope list and the statistics are two reads,
so a scope created between them has no statistics. It is reported with
`stats_known` false and no value for `in_use`, `free`, `pending_offers`,
`addresses`, `used_pct` and `free_pct`: they render `unknown`, match no numeric
comparison and write no perfdata, rather than a 0 that would read as an empty,
healthy scope. The default warning (`used_pct > 80 or stats_known = 0`, kept by
`-w`) flags it, and the detail line reads "no statistics". It clears on the
next run.

**Permissions:** the agent's default LocalSystem account can read the server.
Under another account, that account must be a member of *DHCP Users* (or *DHCP
Administrators*); otherwise the check returns **UNKNOWN** with "access denied".

**Failover:** each partner of a DHCP failover relationship reports the scope as
it sees it. Run the check on both partners.

The check reads the local server only, and IPv6 scopes are not covered.
