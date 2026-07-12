**Default check on a clean system:**

```
check_pending_reboot
OK: No reboot pending
```

**Default check when a reboot is queued (default `warn=pending = 1`):**

```
check_pending_reboot
WARNING: Reboot required: Windows Update
```

**Escalate a pending reboot to CRITICAL:**

```
check_pending_reboot "crit=pending = 1"
CRITICAL: Reboot required: Component Based Servicing, Windows Update
```

**Only alert on specific causes (ignore Windows Update, alert on servicing or a pending file rename):**

```
check_pending_reboot "warn=none" "crit=servicing = 1 or file_rename = 1"
OK: No reboot pending
```

**Custom output showing the number of signals and the reasons:**

```
check_pending_reboot "top-syntax=%(status): %(list)" "detail-syntax=%(count) signal(s): %(reasons)"
WARNING: 1 signal(s): pending file rename
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_pending_reboot
OK: No reboot pending
```
