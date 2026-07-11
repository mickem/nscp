**Default check on a volume with recent snapshots:**

```
check_shadowcopy
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```

**Default check on a host with no shadow copies (empty state is OK):**

```
check_shadowcopy
OK: No shadow copies found
```

**Require snapshots to exist — alert when there are none:**

```
check_shadowcopy empty-state=critical
CRITICAL: No shadow copies found
```

**Alert when the newest snapshot is stale (weekly schedule):**

```
check_shadowcopy "warning=newest > 8d" "critical=newest > 15d"
OK: \\?\Volume{4c2b...}\: 4 copies, newest 2026-07-09 02:00:01 UTC
```

**Alert when shadow storage is nearly full (oldest restore points about to age out):**

```
check_shadowcopy "warning=used_pct > 80" "critical=used_pct > 95"
WARNING: \\?\Volume{4c2b...}\: 20 copies, newest 2026-07-11 07:00:03 UTC
```

**Require at least a minimum number of restore points per volume:**

```
check_shadowcopy "critical=count < 3"
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```

**Custom output with counts and storage usage:**

```
check_shadowcopy "top-syntax=%(status): %(list)" "detail-syntax=%(volume): %(count) copies, %(used) of %(max_size) used (%(used_pct)%)"
OK: \\?\Volume{4c2b...}\: 12 copies, 1610612736 of 10737418240 used (15%)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_shadowcopy --argument "warning=newest > 26h"
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```
