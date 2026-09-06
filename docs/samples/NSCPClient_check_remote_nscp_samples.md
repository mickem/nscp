**Run a check on a remote NSClient++ agent:**

```
check_remote_nscp host=192.168.56.103 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used|'C:\ used'=91.2GB;80;90;0;100 'C:\ used %'=91%;80;90;0;100
```

Unlike NRPE, the result travels as structured data, so the performance data
arrives intact regardless of length.

**Pass arguments to the remote check (`argument=`, repeatable):**

```
check_remote_nscp host=192.168.56.103 command=check_drivesize "argument=drive=C:" "argument=crit=used > 95%"
OK: OK All 1 drive(s) are ok
```

**Use a configured target instead of spelling out the connection:**

Put the host, password and TLS material under
`[/settings/NSCP/client/targets/...]` so credentials stay out of process
listings:

```ini
[/settings/NSCP/client/targets/web01]
address = nscp://192.168.56.103:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

```
check_remote_nscp target=web01 command=check_uptime
OK: uptime: 12d 04:31h, boot: 2026-08-23 08:29:11 (local)
```

**Nothing listening:**

```
check_remote_nscp host=127.0.0.1 port=15669 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

**No host given:**

The default port is 8443, so a call with no target at all fails against an empty
address rather than doing something surprising:

```
check_remote_nscp
UNKNOWN: Error: Failed to connect to: :8443 :Address family not supported by protocol
```

**Long output survives:**

This is the main practical difference from NRPE, whose version-2 protocol
truncates at a fixed 1024-byte payload:

```
check_remote_nscp target=web01 command=check_files "argument=path=C:\logs" "argument=top-syntax=${list}"
OK: app-2026-09-01.log, app-2026-09-02.log, app-2026-09-03.log, ... (412 files)
```
