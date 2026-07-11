To check the size of **the C:\ drive** and **make sure it has at least 10% free** space:

```
check_drivesize "crit=free<10%" drive=c:
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of **all the drives** and make sure it has at least 10% free space:

```
check_drivesize "crit=free<10%" drive=*
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

To scan **all drives** but **require that specific drives are present** — going
CRITICAL if a mandatory drive is missing:

```
    check_drivesize drive=* require=D: require=E: "crit=free<10%"
CRITICAL: Required drive(s) not found: E: | OK: All drives ok
```

`require` (alias `mandatory-drives`) can be repeated and matches by drive letter
(with or without the trailing colon), volume label, or volume id. It is the one
`UsedPartitionSpace` feature gap that wildcard scanning alone could not cover:
`drive=*` silently reports OK when an expected disk has vanished, whereas
`require=` makes that a hard CRITICAL.

To check the size of all the drives and **display all values, not just problems**:

```
check_drivesize drive=* --show-all
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of all the drives and **return the value in gigabytes**. 
By default, units on performance data will be scaled to "something appropriate":

```
check_drivesize "perf-config=*(unit:g)"
L        cli CRITICAL: CRITICAL C:\\: 208.147GB/223.471GB used, D:\\: 399.607GB/465.759GB used
L        cli  Performance data: 'C:\ used'=0.00019g;0.00017;0.00019;0;0.00021 'C:\ used %'=93%;79;89;0;100 'D:\ used'=0.00038g;0.00035;0.00039;0;0.00044 'D:\ used %'=85%;79;89;0;100 'E:\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used %'=33%;79;89;0;100
```

To check the size of **a mounted volume** (c:\volume_test) and **make sure it has 1M free** space **warn if free space is less than 10M**:

```
   check_drivesize "crit=free<1M" "warn=free<10M" drive=c:\\volume_test
   C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;
```

To check the size of **all volumes** and **make sure they have 1M space free**:

```
check_drivesize "crit=free<1M" drive=all-volumes
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'E:\ free'=0B;0;0;0;0 'F:\ free'=0B;0;0;0;0
```

To check the size of **all fixed and network drives** and make sure they have at least 1gig free space:

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')"
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```


To check **all fixed and network drives but ignore C and F**:

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')" exclude=C:\\ exclude=D:\\
L     client OK: All drives ok
L     client  Performance data: 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

To **restrict by filesystem type** — for example, only NTFS volumes — use the
`filesystem` keyword (alias `fs`). The value compared against is whatever the
OS reports via `GetVolumeInformation`, typically uppercase: `NTFS`, `FAT32`,
`exFAT`, `ReFS`, `CDFS`, `UDF`. Empty string is reported for unmounted or
unreadable volumes.

```
check_drivesize drive=* "filter=fs = 'NTFS'"
L     client OK: All drives ok
L     client  Performance data: 'C:\ used'=205GB;...
```

Combine with `type` to scope further — for example, only **fixed** disks that
are **NTFS or ReFS**:

```
check_drivesize drive=* "filter=type = 'fixed' and fs in ('NTFS', 'ReFS')"
```

Use `like` for **case-insensitive** matching, since the OS reports uppercase
but a recipe written as `'ntfs'` should still work:

```
check_drivesize drive=* "filter=filesystem like 'ntfs'"
```

Drop volumes whose filesystem could not be read (e.g. an empty CD/DVD drive):

```
check_drivesize drive=* "filter=fs != ''"
```

Default via NRPE:

```
check_nrpe --host 192.168.56.103 --command check_drivesize
C:\: 205GB/223GB used, D:\: 448GB/466GB used, M:\: 2.6TB/2.68TB used|'C:\ used'=204GB;44;22;0;223 'C:\ used %'=91%;19;9;0;100 'D:\ used'=447GB;93;46;0;465...
```

**Check inode exhaustion (Linux) — a filesystem can be "not full" on bytes yet out of inodes:**

```
check_drivesize drive=/ "warn=inodes_used_pct > 85" "crit=inodes_used_pct > 95" "detail-syntax=${drive} inodes ${inodes_used}/${inodes_total} (${inodes_used_pct}%)"
OK: / inodes 350474/67108864 (1%)
```

The inode keywords are `inodes_total`, `inodes_free`, `inodes_used`,
`inodes_free_pct` and `inodes_used_pct`.
