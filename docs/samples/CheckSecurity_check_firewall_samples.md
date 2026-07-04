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
