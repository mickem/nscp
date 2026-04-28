#### Checking for OS Package Updates

The `check_os_updates` command allows you to monitor for missing package updates on Linux/Unix systems. It utilizes the system package manager (e.g., `apt`, `dnf`, `yum`, `zypper`, or `pacman`) to determine the pending updates.

**Basic usage**

To simply check if there are any pending updates:

```
check_os_updates
```

If there are any pending updates, this will return a warning state by default (because the default `warning` filter is `count > 0`).

**Checking for critical updates**

Often, you only want to be alerted if there are *security* or *critical* updates missing. You can configure this using the `warning` and `critical` filters:

```
check_os_updates "warning=important > 0" "critical=security > 0 or critical > 0"
```

This will return `WARNING` if there are updates with the 'Important' severity, and `CRITICAL` if there are any security updates or updates explicitly marked 'Critical'.

**Customizing the output**

You can use the syntax options to format the output string. For example, to list out the update titles:

```
check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
```