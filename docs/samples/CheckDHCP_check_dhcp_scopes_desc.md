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

Defaults: **WARNING** when `used_pct > 80`, **CRITICAL** when `used_pct > 95`,
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
not running or not installed on this host". A server with no enabled scopes is
also **UNKNOWN** ("No enabled DHCP scopes found") — a check for exhaustion on a
server that leases nothing is pointed at the wrong host. Pass `empty-state=ok`
where that is expected.

**Permissions:** the agent's default LocalSystem account can read the server.
Under another account, that account must be a member of *DHCP Users* (or *DHCP
Administrators*); otherwise the check returns **UNKNOWN** with "access denied".

**Failover:** each partner of a DHCP failover relationship reports the scope as
it sees it. Run the check on both partners.

The check reads the local server only, and IPv6 scopes are not covered.
