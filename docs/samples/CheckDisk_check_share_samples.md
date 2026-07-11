**List all shares on the host:**

```
check_share
OK: All 3 share(s) ok.|'count'=3
```

**Verify a required share exists (present → OK):**

```
check_share share=C$
OK: C$ (type=disk, path=C:\, exists=1)|'count'=1
```

**Verify a required share exists (missing → CRITICAL):**

```
check_share share=Public
CRITICAL: Public (type=disk, path=, exists=0)|'count'=1
```

**Require several shares at once:**

```
check_share share=Public share=Profiles share=Software
OK: All 3 share(s) ok.|'count'=3
```

**Alert if any non-administrative share is unexpectedly published:**

```
check_share "crit=is_admin = 0" "top-syntax=%(status): %(problem_list)" "detail-syntax=%(name) -> %(path)"
OK: All 5 share(s) ok.
```

**List only non-admin (user-created) shares with their paths:**

```
check_share "filter=is_admin = 0" "top-syntax=%(status): %(list)" "detail-syntax=%(name) (%(type)) -> %(path)"
OK: Public (disk) -> C:\Shared, Profiles (disk) -> D:\Profiles
```

**Over NRPE against a file server:**

```
check_nscp_client --host 192.168.56.103 --command check_share --argument "share=Public" --argument "share=Profiles"
OK: All 2 share(s) ok.
```
