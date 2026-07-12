**Default check (reports install count and how long since the newest hotfix):**

```
check_patch_age
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Warn if the box has not been patched in 40 days, critical after 90:**

```
check_patch_age "warn=age > 40" "crit=age > 90"
WARNING: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (57d ago)
```

**Verify a specific hotfix is installed (vulnerability response) — CRITICAL if missing:**

```
check_patch_age hotfix=KB5034441
CRITICAL: 42 hotfixes installed, newest KB5030211 on 1/9/2024 (94d ago); missing: KB5034441
```

**Verify several required hotfixes at once (bare numbers get an implicit KB prefix):**

```
check_patch_age hotfix=KB5034441 hotfix=5030211
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Test presence via the `ids` list instead of the `hotfix=` option:**

```
check_patch_age "crit=ids not like 'KB5034441'"
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Custom output listing the newest hotfix only:**

```
check_patch_age "top-syntax=%(status): %(list)" "detail-syntax=newest %(newest_id) (%(age)d ago), %(count) installed"
OK: newest KB5034441 (18d ago), 42 installed
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_patch_age --argument "warn=age > 40"
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```
