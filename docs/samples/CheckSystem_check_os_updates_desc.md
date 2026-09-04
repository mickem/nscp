#### Checking for pending OS updates

`check_os_updates` reports the updates the system is waiting to install. The
counters (`updates`, `security`, …) are **record keywords**: reference them from
`detail-syntax` (rendered per record and included in `${list}`), not from
`top-syntax`, where they read as 0. On both platforms the default `warning`
filter is `updates > 0`, so a bare call warns whenever anything is pending:

```
check_os_updates
```

##### Windows

Sourced from the Windows Update Agent (WUA) API. Results can be filtered by
severity, reboot requirements and other attributes.

**Checking for critical updates**

Often, you only want to be alerted if there are *security* or *critical* updates missing. You can configure this using the `warning` and `critical` filters:

```
check_os_updates "warning=important > 0" "critical=security > 0 or critical > 0"
```

This will return `WARNING` if there are updates with the 'Important' severity, and `CRITICAL` if there are any security updates or updates explicitly marked 'Critical'.

**Checking if a reboot is required**

If you want to know if the system needs a reboot after installing updates:

```
check_os_updates "warning=reboot_required > 0"
```

`reboot_required` counts updates that *would* require a reboot once installed.
To detect a reboot that is *already pending* system-wide — including reboots
queued by updates that have already been installed (which `reboot_required` no
longer reflects) — use `reboot_pending`, sourced from the Windows Update
`RebootRequired` registry key:

```
check_os_updates "crit=reboot_pending = 1" "detail-syntax=reboot pending: ${reboot_pending}"
```

**Defender / definition and rollup categories**

Defender/antivirus definition updates churn several times a day, so most admins
threshold them separately from OS patches. `defender` counts updates in the
`Definition Updates` / `Microsoft Defender Antivirus` categories, and `rollups`
counts monthly `Update Rollup` updates:

```
check_os_updates "warning=updates - defender > 0" "detail-syntax=${updates} total, ${defender} defender, ${rollups} rollups"
```

**Filtering by title**

`update-filter=<substring>` restricts the check to updates whose title contains
the (case-insensitive) substring; all counters (`updates`, `security`, …) are then
recomputed over just the matching subset:

```
check_os_updates update-filter=".NET" "detail-syntax=${updates} .NET updates: ${titles}"
```

> **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
> excluded** by design. This keeps the count focused on OS/application patches.

##### Linux

Sourced from the system package manager — `apt`, `dnf`, `yum`, `zypper` or
`pacman`, whichever the host uses. The `manager` keyword names the one that was
queried, and `count` remains a deprecated alias for `updates`.

**Checking for security updates only**

Often, you only want to be alerted for *security* updates. You can configure this using the `warning` and `critical` filters:

```
check_os_updates "warning=none" "critical=security > 0"
```

This will return `CRITICAL` if any security updates are pending and otherwise `OK` regardless of the number of ordinary updates.

##### Customizing the output

You can use the syntax options to format the output string:

```
check_os_updates "top-syntax=${status}: ${list}" "detail-syntax=Found ${updates} missing updates. Security: ${security}, Critical: ${critical} - ${titles}"
```

On Linux, list the pending package names and the package manager that reported
them:

```
check_os_updates "detail-syntax=${updates} updates via ${manager}: ${packages}" show-all
```
