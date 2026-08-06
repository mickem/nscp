**Verify that a disk is writable (write, read back and delete a 1k test file):**

```
check_disk_write file=/tmp/nscp-write-test.dat
OK: /tmp/nscp-write-test.dat: wrote and read back 1024 bytes in 4ms
```

On Windows use a path on the drive you want to test:

```
check_disk_write file=D:\temp\nscp-write-test.dat
OK: D:\temp\nscp-write-test.dat: wrote and read back 1024 bytes in 4ms
```

**Write more data and alert when the round-trip gets slow:**

```
check_disk_write file=/tmp/nscp-write-test.dat size=4M "warning=total_time > 1000" "critical=total_time > 5000"
OK: /tmp/nscp-write-test.dat: wrote and read back 4194304 bytes in 14ms|'/tmp/nscp-write-test.dat total_time'=14ms;1000;5000
```

**A target that cannot be written to is CRITICAL:**

```
check_disk_write file=/root/nscp-write-test.dat
CRITICAL: /root/nscp-write-test.dat: failed to create file: Permission denied
```

```
check_disk_write file=/no/such/dir/nscp-write-test.dat
CRITICAL: /no/such/dir/nscp-write-test.dat: failed to create file: No such file or directory
```

**The check never touches a file it did not create itself:**

```
check_disk_write file=/tmp/nscp-existing.dat
CRITICAL: /tmp/nscp-existing.dat: file already exists (refusing to overwrite it)
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_disk_write --argument "file=/data/nscp-write-test.dat" --argument "size=1M"
OK: /data/nscp-write-test.dat: wrote and read back 1048576 bytes in 9ms
```
