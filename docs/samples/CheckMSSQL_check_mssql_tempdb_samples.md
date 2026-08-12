**Default check (informational, everything as perfdata):**

```
check_mssql_tempdb
OK: tempdb 4% used of 67108864B (version store 0B, user 1572864B, internal 0B)|'tempdb_free'=64094208B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=67108864B;0;0 'tempdb_used'=3014656B;0;0 'tempdb_used_pct'=4%;0;0 'tempdb_user_objects'=1572864B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=992251518976B;0;0
```

**Usage and volume thresholds:**

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 90 or volume_free < 1G"
OK: tempdb 4% used of 67108864B (version store 0B, user 1572864B, internal 0B)|'tempdb_free'=64094208B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=67108864B;0;0 'tempdb_used'=3014656B;0;0 'tempdb_used_pct'=4%;80;90 'tempdb_user_objects'=1572864B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=992251518976B;0;1073741824
```

**During temp-table pressure — the split shows the consumer:**

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 95"
OK: tempdb 24% used of 603979776B (version store 0B, user 144310272B, internal 0B)|'tempdb_free'=457900032B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=603979776B;0;0 'tempdb_used'=146079744B;0;0 'tempdb_used_pct'=24%;80;95 'tempdb_user_objects'=144310272B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=991714615296B;0;0
```

Here a session holding a large temp table grew tempdb from 64MB to 576MB and
`user_objects` accounts for nearly all of the usage — a temp-table problem,
not a version-store or spill problem.

**Catch a snapshot transaction pinning the version store:**

```
check_mssql_tempdb "warning=version_store > 5G"
OK: tempdb 24% used of 603979776B (version store 0B, user 144310272B, internal 0B)|'tempdb_free'=457900032B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=603979776B;0;0 'tempdb_used'=146079744B;0;0 'tempdb_used_pct'=24%;0;0 'tempdb_user_objects'=144310272B;0;0 'tempdb_version_store'=0B;5368709120;0 'tempdb_volume_free'=991714598912B;0;0
```
