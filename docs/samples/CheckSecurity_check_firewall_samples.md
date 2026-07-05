#### Check that all Windows firewall profiles are enabled (Windows only)

By default the check is critical if any of the three profiles (Domain, Private,
Public) has its firewall disabled.

```
check_firewall
L        cli OK: all 3 firewall profile(s) enabled
```

```
check_firewall
L        cli CRITICAL: Public=0
```

#### Only require a specific profile to be enabled

```
check_firewall "filter=profile = 'Domain'" crit=enabled=0
L        cli OK: all 1 firewall profile(s) enabled
```

#### Warn when the machine is on the Public profile

Network Location Awareness can silently re-categorise a network to *public*
after a router or connection change; rules scoped to the domain/private
profiles then stop applying and services get blocked. Warn on that (opt-in —
on e.g. laptops the public profile is normal):

```
check_firewall "warn=active = 1 and profile = 'Public'" "detail-syntax=${profile} profile is active"
L        cli WARNING: Public profile is active
```

The `active` flags are also available for display and perfdata:

```
check_firewall "detail-syntax=${profile}: enabled=${enabled} active=${active}" top-syntax=${list}
L        cli OK: Domain: enabled=1 active=0, Private: enabled=1 active=0, Public: enabled=1 active=1
```

#### Show the default inbound/outbound actions

```
check_firewall "detail-syntax=${profile}: enabled=${enabled} in=${inbound} out=${outbound}" top-syntax=${list}
L        cli OK: Domain: enabled=1 in=block out=allow, Private: enabled=1 in=block out=allow, Public: enabled=1 in=block out=allow
```

#### On non-Windows platforms

`check_firewall` models the Windows three-profile firewall and is not
implemented on Linux (whose firewalld/ufw/nftables model differs):

```
check_firewall
L        cli UNKNOWN: check_firewall is not supported on this platform (Windows-only; the Linux firewall model differs)
```
