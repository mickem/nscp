##### Linux

**Default check (any pending update warns):**

```
check_os_updates
CRITICAL: 176 updates available (152 security) via apt|'updates_security'=152;0;0 'updates'=176;0;0
```

The default critical threshold is `security > 0`, so a host with pending
security updates goes critical rather than merely warning.

**Only care about security updates:**

```
check_os_updates "warning=none" "critical=security > 0"
CRITICAL: 176 updates available (152 security) via apt|'updates_security'=152;0;0
```

**Tolerate a backlog of ordinary updates:**

```
check_os_updates "warning=updates > 200" "critical=updates > 500"
OK: 176 updates available (152 security) via apt|'updates'=176;200;500
```

**Show which package manager answered:**

```
check_os_updates "detail-syntax=${updates} updates via ${manager}"
CRITICAL: 176 updates via apt|'updates_security'=152;0;0 'updates'=176;0;0
```

**List the pending package names:**

```
check_os_updates "detail-syntax=${updates}: ${packages}" show-all
CRITICAL: 176: bsdutils, bzip2, ca-certificates, containerd.io, coreutils, curl, diffutils, dirmngr, distro-info-data, docker-buildx-plugin, ...
```

##### Windows

**Default check:**

```
check_os_updates
WARNING: 7 updates available (2 security)|'updates'=7;0;0 'updates_security'=2;0;0
```

**Alert only on security and critical updates:**

```
check_os_updates "warning=important > 0" "critical=security > 0 or critical > 0"
CRITICAL: 7 updates available (2 security)
```

**Detect a pending reboot:**

`reboot_required` counts updates that would need a reboot once installed;
`reboot_pending` reports a reboot that is *already* queued system-wide.

```
check_os_updates "crit=reboot_pending = 1" "detail-syntax=reboot pending: ${reboot_pending}"
CRITICAL: reboot pending: 1
```

**Threshold OS patches separately from Defender definitions:**

```
check_os_updates "warning=updates - defender > 0" "detail-syntax=${updates} total, ${defender} defender, ${rollups} rollups"
WARNING: 7 total, 3 defender, 1 rollups
```

**Restrict to updates whose title matches a substring:**

All the counters are recomputed over just the matching subset.

```
check_os_updates update-filter=".NET" "detail-syntax=${updates} .NET updates: ${titles}"
WARNING: 2 .NET updates: 2026-08 Cumulative Update for .NET Framework 4.8, Update for .NET 8.0.14
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_os_updates --arguments "critical=security > 0"
OK: 0 updates available (0 security)
```
