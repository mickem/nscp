**Check free space on a UNC path:**

```
check_uncpath path=\\fileserver\data "crit=used_pct > 90"
OK: \\fileserver\data: 1.2T/2.0T used (800G free)
'\\fileserver\data used_pct'=60%;80;90 '\\fileserver\data free'=800G;;
```

Unlike `check_drivesize` (which only sees OS-mounted drives), `check_uncpath`
takes an arbitrary `\\server\share` and reports quota-aware free space.

**With alternate credentials (Windows):**

```
check_uncpath path=\\fileserver\backups user=DOMAIN\svc password=secret "crit=free < 100g"
OK: \\fileserver\backups: 400G/2.0T used (1.6T free)
```

`user`/`password` map to a temporary `WNetAddConnection2` before the query and
are disconnected afterwards. `user_free` reports the free space available to the
querying account (honouring per-user quotas) as distinct from the share's total
`free`.

**Multiple paths:**

```
check_uncpath path=\\a\share path=\\b\share "crit=free_pct < 10" "top-syntax=${status}: ${problem_list}"
```
