#### Checking for OS Package Updates

The `check_os_updates` command allows you to monitor for missing package updates on Linux/Unix systems. It utilizes the system package manager (e.g., `apt`, `dnf`, `yum`, `zypper`, or `pacman`) to determine the pending updates.

**Basic usage**

To simply check if there are any pending updates:

```
check_os_updates
```

If there are any pending updates, this will return a warning state by default (because the default `warning` filter is `updates > 0`; `count` remains a deprecated alias for `updates`).

**Checking for security updates only**

Often, you only want to be alerted for *security* updates. You can configure this using the `warning` and `critical` filters:

```
check_os_updates "warning=none" "critical=security > 0"
```

This will return `CRITICAL` if any security updates are pending and otherwise `OK` regardless of the number of ordinary updates.

**Customizing the output**

You can use the syntax options to format the output string. For example, to list the pending package names (note that `updates`/`security` are record keywords, so reference them in `detail-syntax` — in `top-syntax` they render as 0):

```
check_os_updates "detail-syntax=${updates} updates via ${manager}: ${packages}" show-all
```