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

**Customizing the output**

You can use the syntax options to format the output string:

```
check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
```
