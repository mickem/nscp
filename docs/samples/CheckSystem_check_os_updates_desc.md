#### Checking for Windows Updates

The `check_os_updates` command allows you to monitor for missing Windows updates via the Windows Update Agent (WUA) API. You can filter the results based on severity, reboot requirements, and other attributes. 

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
check_os_updates "warning=count - defender > 0" "detail-syntax=${count} total, ${defender} defender, ${rollups} rollups"
```

**Filtering by title**

`update-filter=<substring>` restricts the check to updates whose title contains
the (case-insensitive) substring; all counters (`count`, `security`, …) are then
recomputed over just the matching subset:

```
check_os_updates update-filter=".NET" "detail-syntax=${count} .NET updates: ${titles}"
```

> **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
> excluded** by design. This keeps the count focused on OS/application patches.

**Customizing the output**

You can use the syntax options to format the output string:

```
check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
```
