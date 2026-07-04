**Check that every real filesystem is mounted as expected:**

```
check_mount
OK: mounts are as expected
```

**Check a single mount point:**

```
check_mount mount=/
OK: mounts are as expected
```

**Require a specific filesystem type (warns when it differs):**

```
check_mount mount=/ fstype=zfs
WARNING: mount / expected fstype differs: zfs != ext4
```

**Require specific mount options (e.g. that `/` is mounted read-write with `noatime`):**

```
check_mount mount=/ options=rw,noatime
WARNING: mount / missing options: noatime
```

**A mount point that is not mounted is CRITICAL:**

```
check_mount mount=/does/not/exist
CRITICAL: mount /does/not/exist not mounted
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_mount --argument "mount=/data" --argument "fstype=ext4"
OK: mounts are as expected
```
