**Default check on a clean system:**

```
check_pending_reboot
OK: No reboot pending
```

**Default check when a reboot is queued (default `warn=pending = 1`):**

```
check_pending_reboot
WARNING: Reboot required: Windows Update (pending since 2026-08-16 09:41:12)
```

**Warn on any pending reboot but escalate one that has been pending for over a week:**

```
check_pending_reboot "warn=pending = 1" "crit=pending = 1 and age > 7d"
CRITICAL: Reboot required: Windows Update (pending since 2026-08-10 03:12:45)
```

The since-time is the last-write time of the Component Based Servicing /
Windows Update registry key, which exists only while that reboot is queued.
The other signals (file rename, computer rename, domain join) carry no
timestamp, so `age` and `written` report `unknown` for them and never trip a
numeric threshold (test for it with `written = 'unknown'`).

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
check_pending_reboot "top-syntax=%(status): %(list)" "detail-syntax=%(signals) signal(s): %(reasons)"
WARNING: 1 signal(s): pending file rename
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_pending_reboot
OK: No reboot pending
```

**Linux: a kernel update and the Debian/Ubuntu marker (default `warn=pending = 1`):**

```
check_pending_reboot
WARNING: Reboot required: reboot-required marker (linux-image-6.18.52-fc-v819, libc6, libssl3t64), newer kernel installed (6.18.52-fc-v819, running 6.18.44-fc-v819) (pending since 2026-11-15 10:19:17)|'reboot_pending'=1;1 'reboot_signals'=2
```

**Linux: escalate after a week, the same expression as on Windows:**

```
check_pending_reboot "warn=pending = 1" "crit=pending = 1 and age > 7d"
CRITICAL: Reboot required: reboot-required marker (linux-image-6.18.52-fc-v819, libc6, libssl3t64), newer kernel installed (6.18.52-fc-v819, running 6.18.44-fc-v819) (pending since 2026-11-15 10:19:17)|'reboot_age'=777600s;;604800 'reboot_pending'=1;1;1 'reboot_signals'=2
```

**Linux: only a kernel change is critical (the marker alone stays quiet):**

```
check_pending_reboot "warn=none" "crit=kernel = 1"
CRITICAL: Reboot required: reboot-required marker (linux-image-6.18.52-fc-v819, libc6, libssl3t64), newer kernel installed (6.18.52-fc-v819, running 6.18.44-fc-v819) (pending since 2026-11-15 10:19:17)|'reboot_kernel'=1;;1 'reboot_pending'=1 'reboot_signals'=2
```

**Linux: what was compared, and which packages asked for the reboot:**

```
check_pending_reboot "detail-syntax=${signals} signal(s): ${reasons}; running=${running_kernel} newest=${newest_kernel}"
WARNING: 2 signal(s): reboot-required marker (linux-image-6.18.52-fc-v819, libc6, libssl3t64), newer kernel installed (6.18.52-fc-v819, running 6.18.44-fc-v819); running=6.18.44-fc-v819 newest=6.18.52-fc-v819

check_pending_reboot "detail-syntax=${packages}"
WARNING: linux-image-6.18.52-fc-v819, libc6, libssl3t64
```

**Linux container (no kernel package for the host's kernel, no marker):**

```
check_pending_reboot "detail-syntax=${signals} signal(s): ${reasons}; running=${running_kernel} newest=${newest_kernel}"
OK: 0 signal(s): none; running=6.18.44-fc-v819 newest=
```
