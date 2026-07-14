# CheckDisk

CheckDisk can check various file and disk related things.

`CheckDisk` is provides two disk related checks one for checking size of drives and the other for checking status of files and folders.

<!-- @formatter:off -->
!!! warning "UNC and Network Paths"
    Please note that UNC and network paths are only available in each session meaning a user mounted share will not be visible to NSClient++ (since services run in their own session).
    But as long as NSClient++ can access the share you can still check it as you specify the UNC path.
    In other words the following will **NOT** work: `check_drivesize drive=m:` But the following will: `check_drivesize drive=\\myserver\\mydrive`
<!-- @formatter:on -->


## Enable module

To enable this module and and allow using the commands you need to ass `CheckDisk = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckDisk = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckDisk module.

**List of commands:**

A list of all available queries (check commands)

| Command                                 | Description                                                                                                                                                       |
|-----------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [check_disk_health](#check_disk_health) | Combined per-drive health check (free space + I/O metrics).                                                                                                       |
| [check_disk_io](#check_disk_io)         | Check disk I/O performance metrics (throughput, IOPS, queue length, busy time).                                                                                   |
| [check_drivesize](#check_drivesize)     | Check the size (free-space) of a drive or volume.                                                                                                                 |
| [check_files](#check_files)             | Check various aspects of a file and/or folder.                                                                                                                    |
| [check_mount](#check_mount)             | Check that a filesystem is mounted with the expected fstype and options.                                                                                          |
| [check_shadowcopy](#check_shadowcopy)   | Check VSS shadow-copy (Volume Shadow Copy) recency, count and shadow-storage usage per volume (Windows).                                                          |
| [check_share](#check_share)             | Check Windows SMB shares: list them, or verify that specific required shares exist (Windows).                                                                     |
| [check_single_file](#check_single_file) | Check various aspects of a single file (size, age, line count, version, ...). Simpler alternative to check_files when you only need to inspect one specific file. |
| [check_storagepool](#check_storagepool) | Check Storage Spaces pool health and capacity (Windows).                                                                                                          |
| [check_uncpath](#check_uncpath)         | Check free space on a UNC path (server share), with optional alternate credentials.                                                                               |

### check_disk_health

Combined per-drive health check (free space + I/O metrics).

`check_disk_health` is a combined per-disk health check. It reports three kinds
of row, each judged only on the data that is real for it:

* **Space rows** (`has_space = 1`) — one per mounted filesystem, with
  `free`/`used`/`free_pct`/`used_pct`/`user_free` and the I/O of the backing
  device.
* **I/O rows** (`has_space = 0`, `has_device = 0`) — devices/totals with no
  mounted filesystem (e.g. `_Total`), judged on `percent_disk_time` and queue.
* **Device rows** (`has_device = 1`) — one per physical disk (Windows only,
  from `MSFT_PhysicalDisk` / `MSFT_Disk`), judged on physical-disk health.

### Device-state keywords (Windows)

| Keyword              | Description                                                      |
|----------------------|------------------------------------------------------------------|
| `has_device`         | `1` on a physical-disk row, `0` otherwise (guard; no perfdata).  |
| `friendly_name`      | Physical disk friendly name.                                     |
| `serial`             | Physical disk serial number.                                     |
| `media_type`         | `HDD`, `SSD`, `SCM`, or `Unspecified`.                           |
| `health_status`      | `Healthy`, `Warning`, `Unhealthy`, or `Unknown`.                 |
| `operational_status` | Synthesised single value: `Offline`, `OK`, or the health string. |
| `is_offline`         | `1` if the disk is offline.                                      |
| `is_readonly`        | `1` if the disk is read-only.                                    |
| `disk_number`        | Physical disk number/index.                                      |

Device rows are best-effort: if the `MSFT_PhysicalDisk` / `MSFT_Disk` WMI classes
are unavailable (very old Windows, or a system with no Storage provider), no
device rows are produced and the check still reports space and I/O normally.

### Default thresholds

By default the check is WARNING when a filesystem drops below 20% free, its disk
is over 80% busy, or a physical disk reports `Warning` health; and CRITICAL below
10% free, over 95% busy, or when a physical disk is `Unhealthy` or offline.

**Jump to section:**

* [Sample Commands](#check_disk_health_samples)
* [Command-line Arguments](#check_disk_health_options)
* [Filter keywords](#check_disk_health_filter_keys)


<a id="check_disk_health_samples"></a>
#### Sample Commands

**Default check:**

```
check_disk_health
OK: All disks are healthy.
'C: free_pct'=61%;20;10 'C: percent_disk_time'=2%;80;95 ...
```

**Physical-disk device health:**

`check_disk_health` appends one row per physical disk (from `MSFT_PhysicalDisk` /
`MSFT_Disk`), carrying device state. These rows are identified by `has_device = 1`
and by default go CRITICAL on an unhealthy or offline disk and WARNING on a disk
reporting `Warning` health.

```
check_disk_health "filter=has_device = 1" "detail-syntax=${friendly_name} [${media_type}]: ${health_status}, ${operational_status}"
OK: Samsung SSD 980 [SSD]: Healthy, OK, WDC WD40 [HDD]: Healthy, OK
```

Alerting only on SSD wear / disk failure across all physical disks:

```
check_disk_health "filter=has_device = 1" "crit=health_status != 'Healthy' or is_offline = 1"
CRITICAL: WDC WD40 [HDD]: Unhealthy, Unhealthy
```

Device-state keywords (populated on `has_device = 1` rows): `friendly_name`,
`serial`, `media_type` (HDD/SSD/SCM), `health_status`
(Healthy/Warning/Unhealthy/Unknown), `operational_status`, `is_offline`,
`is_readonly`, `disk_number`.



<a id="check_disk_health_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_disk_health_warn"></a>
    <a id="check_disk_health_crit"></a>
    <a id="check_disk_health_debug"></a>
    <a id="check_disk_health_show-all"></a>
    <a id="check_disk_health_escape-html"></a>
    <a id="check_disk_health_help"></a>
    <a id="check_disk_health_help-pb"></a>
    <a id="check_disk_health_show-default"></a>
    <a id="check_disk_health_help-short"></a>

    | Option                                            | Default Value                                                                                                                       | Description                                                                                                      |
    |---------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_disk_health_filter)               | name != '_Total'                                                                                                                    | Filter which marks interesting items.                                                                            |
    | [warning](#check_disk_health_warning)             | (has_space = 1 and free_pct < 20) or percent_disk_time > 80 or (has_device = 1 and health_status = 'Warning')                       | Filter which marks items which generates a warning state.                                                        |
    | warn                                              |                                                                                                                                     | Short alias for warning                                                                                          |
    | [critical](#check_disk_health_critical)           | (has_space = 1 and free_pct < 10) or percent_disk_time > 95 or (has_device = 1 and (health_status = 'Unhealthy' or is_offline = 1)) | Filter which marks items which generates a critical state.                                                       |
    | crit                                              |                                                                                                                                     | Short alias for critical.                                                                                        |
    | [ok](#check_disk_health_ok)                       |                                                                                                                                     | Filter which marks items which generates an ok state.                                                            |
    | debug                                             | N/A                                                                                                                                 | Show debugging information in the log                                                                            |
    | show-all                                          | N/A                                                                                                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_disk_health_empty-state)     | critical                                                                                                                            | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_disk_health_perf-config)     |                                                                                                                                     | Performance data generation configuration                                                                        |
    | escape-html                                       | N/A                                                                                                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                              | N/A                                                                                                                                 | Show help screen (this screen)                                                                                   |
    | help-pb                                           | N/A                                                                                                                                 | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                      | N/A                                                                                                                                 | Show default values for a given command                                                                          |
    | help-short                                        | N/A                                                                                                                                 | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_disk_health_top-syntax)       | ${status}: ${list}                                                                                                                  | Top level syntax.                                                                                                |
    | [ok-syntax](#check_disk_health_ok-syntax)         | %(status): All disks are healthy.                                                                                                   | ok syntax.                                                                                                       |
    | [empty-syntax](#check_disk_health_empty-syntax)   |                                                                                                                                     | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_disk_health_detail-syntax) | ${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}                                              | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_disk_health_perf-syntax)     | ${name}                                                                                                                             | Performance alias syntax.                                                                                        |



    <h5 id="check_disk_health_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `name != '_Total'`

    <h5 id="check_disk_health_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `(has_space = 1 and free_pct < 20) or percent_disk_time > 80 or (has_device = 1 and health_status = 'Warning')`

    <h5 id="check_disk_health_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `(has_space = 1 and free_pct < 10) or percent_disk_time > 95 or (has_device = 1 and (health_status = 'Unhealthy' or is_offline = 1))`

    <h5 id="check_disk_health_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_disk_health_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `critical`

    <h5 id="check_disk_health_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_disk_health_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_disk_health_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All disks are healthy.`

    <h5 id="check_disk_health_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.


    <h5 id="check_disk_health_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}`

    <h5 id="check_disk_health_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

=== "Linux"

    <a id="check_disk_health_warn"></a>
    <a id="check_disk_health_crit"></a>
    <a id="check_disk_health_debug"></a>
    <a id="check_disk_health_show-all"></a>
    <a id="check_disk_health_escape-html"></a>
    <a id="check_disk_health_help"></a>
    <a id="check_disk_health_help-pb"></a>
    <a id="check_disk_health_show-default"></a>
    <a id="check_disk_health_help-short"></a>

    | Option                                            | Default Value                                                                          | Description                                                                                                      |
    |---------------------------------------------------|----------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_disk_health_filter)               | name != '_Total'                                                                       | Filter which marks interesting items.                                                                            |
    | [warning](#check_disk_health_warning)             | (has_space = 1 and free_pct < 20) or percent_disk_time > 80                            | Filter which marks items which generates a warning state.                                                        |
    | warn                                              |                                                                                        | Short alias for warning                                                                                          |
    | [critical](#check_disk_health_critical)           | (has_space = 1 and free_pct < 10) or percent_disk_time > 95                            | Filter which marks items which generates a critical state.                                                       |
    | crit                                              |                                                                                        | Short alias for critical.                                                                                        |
    | [ok](#check_disk_health_ok)                       |                                                                                        | Filter which marks items which generates an ok state.                                                            |
    | debug                                             | N/A                                                                                    | Show debugging information in the log                                                                            |
    | show-all                                          | N/A                                                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_disk_health_empty-state)     | critical                                                                               | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_disk_health_perf-config)     |                                                                                        | Performance data generation configuration                                                                        |
    | escape-html                                       | N/A                                                                                    | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                              | N/A                                                                                    | Show help screen (this screen)                                                                                   |
    | help-pb                                           | N/A                                                                                    | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                      | N/A                                                                                    | Show default values for a given command                                                                          |
    | help-short                                        | N/A                                                                                    | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_disk_health_top-syntax)       | ${status}: ${list}                                                                     | Top level syntax.                                                                                                |
    | [ok-syntax](#check_disk_health_ok-syntax)         | %(status): All disks are healthy.                                                      | ok syntax.                                                                                                       |
    | [empty-syntax](#check_disk_health_empty-syntax)   |                                                                                        | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_disk_health_detail-syntax) | ${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops} | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_disk_health_perf-syntax)     | ${name}                                                                                | Performance alias syntax.                                                                                        |



    <h5 id="check_disk_health_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `name != '_Total'`

    <h5 id="check_disk_health_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `(has_space = 1 and free_pct < 20) or percent_disk_time > 80`

    <h5 id="check_disk_health_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `(has_space = 1 and free_pct < 10) or percent_disk_time > 95`

    <h5 id="check_disk_health_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_disk_health_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `critical`

    <h5 id="check_disk_health_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_disk_health_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_disk_health_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All disks are healthy.`

    <h5 id="check_disk_health_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.


    <h5 id="check_disk_health_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}: ${free_pct}% free, ${percent_disk_time}% busy, q=${queue_length} iops=${iops}`

    <h5 id="check_disk_health_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`


<a id="check_disk_health_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option              | Description                                                                                                    |
    |---------------------|----------------------------------------------------------------------------------------------------------------|
    | disk_number         | Physical disk number/index (device rows)                                                                       |
    | free                | Free disk space in bytes                                                                                       |
    | free_pct            | Percentage of free disk space                                                                                  |
    | friendly_name       | Physical disk friendly name (device rows)                                                                      |
    | has_device          | 1 if the row carries physical-disk device state (a per-disk row), 0 otherwise                                  |
    | has_space           | 1 if the row has filesystem space data, 0 for I/O-only rows (e.g. _Total or a disk with no mounted filesystem) |
    | health_status       | Physical disk health: Healthy, Warning, Unhealthy or Unknown (device rows)                                     |
    | iops                | Total IOPS (reads + writes)                                                                                    |
    | is_offline          | 1 if the physical disk is offline (device rows)                                                                |
    | is_readonly         | 1 if the physical disk is read-only (device rows)                                                              |
    | media_type          | Physical disk media type: HDD, SSD, SCM or Unspecified (device rows)                                           |
    | name                | Drive name (e.g. C:, D:, _Total)                                                                               |
    | operational_status  | Physical disk operational status: OK, Offline, ... (device rows)                                               |
    | percent_disk_time   | Percent of time the disk is busy                                                                               |
    | percent_idle_time   | Percent of time the disk is idle                                                                               |
    | queue_length        | Current disk queue length                                                                                      |
    | read_bytes_per_sec  | Bytes read per second                                                                                          |
    | reads_per_sec       | Read IOPS                                                                                                      |
    | serial              | Physical disk serial number (device rows)                                                                      |
    | split_io_per_sec    | Split I/O operations per second                                                                                |
    | total_bytes_per_sec | Total bytes per second (read + write)                                                                          |
    | used                | Used disk space in bytes                                                                                       |
    | used_pct            | Percentage of used disk space                                                                                  |
    | user_free           | Free disk space available to current user in bytes                                                             |
    | write_bytes_per_sec | Bytes written per second                                                                                       |
    | writes_per_sec      | Write IOPS                                                                                                     |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

=== "Linux"

    | Option              | Description                                                                                                    |
    |---------------------|----------------------------------------------------------------------------------------------------------------|
    | free                | Free disk space in bytes                                                                                       |
    | free_pct            | Percentage of free disk space                                                                                  |
    | has_space           | 1 if the row has filesystem space data, 0 for I/O-only rows (e.g. _Total or a disk with no mounted filesystem) |
    | iops                | Total IOPS (reads + writes)                                                                                    |
    | name                | Drive name (e.g. C:, D:, _Total)                                                                               |
    | percent_disk_time   | Percent of time the disk is busy                                                                               |
    | percent_idle_time   | Percent of time the disk is idle                                                                               |
    | queue_length        | Current disk queue length                                                                                      |
    | read_bytes_per_sec  | Bytes read per second                                                                                          |
    | reads_per_sec       | Read IOPS                                                                                                      |
    | split_io_per_sec    | Split I/O operations per second                                                                                |
    | total_bytes_per_sec | Total bytes per second (read + write)                                                                          |
    | used                | Used disk space in bytes                                                                                       |
    | used_pct            | Percentage of used disk space                                                                                  |
    | user_free           | Free disk space available to current user in bytes                                                             |
    | write_bytes_per_sec | Bytes written per second                                                                                       |
    | writes_per_sec      | Write IOPS                                                                                                     |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

### check_disk_io

Check disk I/O performance metrics (throughput, IOPS, queue length, busy time).

**Jump to section:**

* [Command-line Arguments](#check_disk_io_options)
* [Filter keywords](#check_disk_io_filter_keys)



<a id="check_disk_io_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_disk_io_warn"></a>
    <a id="check_disk_io_crit"></a>
    <a id="check_disk_io_debug"></a>
    <a id="check_disk_io_show-all"></a>
    <a id="check_disk_io_escape-html"></a>
    <a id="check_disk_io_help"></a>
    <a id="check_disk_io_help-pb"></a>
    <a id="check_disk_io_show-default"></a>
    <a id="check_disk_io_help-short"></a>

    | Option                                        | Default Value                                                                                                        | Description                                                                                                      |
    |-----------------------------------------------|----------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_disk_io_filter)               | name != '_Total'                                                                                                     | Filter which marks interesting items.                                                                            |
    | [warning](#check_disk_io_warning)             | percent_disk_time > 80                                                                                               | Filter which marks items which generates a warning state.                                                        |
    | warn                                          |                                                                                                                      | Short alias for warning                                                                                          |
    | [critical](#check_disk_io_critical)           | percent_disk_time > 95                                                                                               | Filter which marks items which generates a critical state.                                                       |
    | crit                                          |                                                                                                                      | Short alias for critical.                                                                                        |
    | [ok](#check_disk_io_ok)                       |                                                                                                                      | Filter which marks items which generates an ok state.                                                            |
    | debug                                         | N/A                                                                                                                  | Show debugging information in the log                                                                            |
    | show-all                                      | N/A                                                                                                                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_disk_io_empty-state)     | critical                                                                                                             | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_disk_io_perf-config)     |                                                                                                                      | Performance data generation configuration                                                                        |
    | escape-html                                   | N/A                                                                                                                  | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                          | N/A                                                                                                                  | Show help screen (this screen)                                                                                   |
    | help-pb                                       | N/A                                                                                                                  | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                  | N/A                                                                                                                  | Show default values for a given command                                                                          |
    | help-short                                    | N/A                                                                                                                  | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_disk_io_top-syntax)       | ${status}: ${list}                                                                                                   | Top level syntax.                                                                                                |
    | [ok-syntax](#check_disk_io_ok-syntax)         | %(status): All disk I/O seems ok.                                                                                    | ok syntax.                                                                                                       |
    | [empty-syntax](#check_disk_io_empty-syntax)   |                                                                                                                      | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_disk_io_detail-syntax) | ${name}: ${percent_disk_time}% busy, read=${read_bytes_per_sec}B/s write=${write_bytes_per_sec}B/s q=${queue_length} | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_disk_io_perf-syntax)     | ${name}                                                                                                              | Performance alias syntax.                                                                                        |



    <h5 id="check_disk_io_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `name != '_Total'`

    <h5 id="check_disk_io_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `percent_disk_time > 80`

    <h5 id="check_disk_io_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `percent_disk_time > 95`

    <h5 id="check_disk_io_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_disk_io_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `critical`

    <h5 id="check_disk_io_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_disk_io_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_disk_io_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All disk I/O seems ok.`

    <h5 id="check_disk_io_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.


    <h5 id="check_disk_io_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}: ${percent_disk_time}% busy, read=${read_bytes_per_sec}B/s write=${write_bytes_per_sec}B/s q=${queue_length}`

    <h5 id="check_disk_io_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

=== "Linux"

    <a id="check_disk_io_warn"></a>
    <a id="check_disk_io_crit"></a>
    <a id="check_disk_io_debug"></a>
    <a id="check_disk_io_show-all"></a>
    <a id="check_disk_io_escape-html"></a>
    <a id="check_disk_io_help"></a>
    <a id="check_disk_io_help-pb"></a>
    <a id="check_disk_io_show-default"></a>
    <a id="check_disk_io_help-short"></a>

    | Option                                        | Default Value                                                                                                        | Description                                                                                                      |
    |-----------------------------------------------|----------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_disk_io_filter)               | name != '_Total'                                                                                                     | Filter which marks interesting items.                                                                            |
    | [warning](#check_disk_io_warning)             | percent_disk_time > 80                                                                                               | Filter which marks items which generates a warning state.                                                        |
    | warn                                          |                                                                                                                      | Short alias for warning                                                                                          |
    | [critical](#check_disk_io_critical)           | percent_disk_time > 95                                                                                               | Filter which marks items which generates a critical state.                                                       |
    | crit                                          |                                                                                                                      | Short alias for critical.                                                                                        |
    | [ok](#check_disk_io_ok)                       |                                                                                                                      | Filter which marks items which generates an ok state.                                                            |
    | debug                                         | N/A                                                                                                                  | Show debugging information in the log                                                                            |
    | show-all                                      | N/A                                                                                                                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_disk_io_empty-state)     | critical                                                                                                             | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_disk_io_perf-config)     |                                                                                                                      | Performance data generation configuration                                                                        |
    | escape-html                                   | N/A                                                                                                                  | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                          | N/A                                                                                                                  | Show help screen (this screen)                                                                                   |
    | help-pb                                       | N/A                                                                                                                  | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                  | N/A                                                                                                                  | Show default values for a given command                                                                          |
    | help-short                                    | N/A                                                                                                                  | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_disk_io_top-syntax)       | ${status}: ${list}                                                                                                   | Top level syntax.                                                                                                |
    | [ok-syntax](#check_disk_io_ok-syntax)         | %(status): All disk I/O seems ok.                                                                                    | ok syntax.                                                                                                       |
    | [empty-syntax](#check_disk_io_empty-syntax)   |                                                                                                                      | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_disk_io_detail-syntax) | ${name}: ${percent_disk_time}% busy, read=${read_bytes_per_sec}B/s write=${write_bytes_per_sec}B/s q=${queue_length} | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_disk_io_perf-syntax)     | ${name}                                                                                                              | Performance alias syntax.                                                                                        |



    <h5 id="check_disk_io_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `name != '_Total'`

    <h5 id="check_disk_io_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `percent_disk_time > 80`

    <h5 id="check_disk_io_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `percent_disk_time > 95`

    <h5 id="check_disk_io_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_disk_io_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `critical`

    <h5 id="check_disk_io_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_disk_io_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_disk_io_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All disk I/O seems ok.`

    <h5 id="check_disk_io_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.


    <h5 id="check_disk_io_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}: ${percent_disk_time}% busy, read=${read_bytes_per_sec}B/s write=${write_bytes_per_sec}B/s q=${queue_length}`

    <h5 id="check_disk_io_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`


<a id="check_disk_io_filter_keys"></a>
#### Filter keywords

| Option              | Description                             |
|---------------------|-----------------------------------------|
| iops                | Total IOPS (reads + writes)             |
| name                | Logical disk name (e.g. C:, D:, _Total) |
| percent_disk_time   | Percent of time the disk is busy        |
| percent_idle_time   | Percent of time the disk is idle        |
| queue_length        | Current disk queue length               |
| read_bytes_per_sec  | Bytes read per second                   |
| reads_per_sec       | Read IOPS                               |
| split_io_per_sec    | Split I/O operations per second         |
| total_bytes_per_sec | Total bytes per second (read + write)   |
| write_bytes_per_sec | Bytes written per second                |
| writes_per_sec      | Write IOPS                              |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

### check_drivesize

Check the size (free-space) of a drive or volume.

**Jump to section:**

* [Sample Commands](#check_drivesize_samples)
* [Command-line Arguments](#check_drivesize_options)
* [Filter keywords](#check_drivesize_filter_keys)


<a id="check_drivesize_samples"></a>
#### Sample Commands

To check the size of **the C:\ drive** and **make sure it has at least 10% free** space:

```
check_drivesize "crit=free<10%" drive=c:
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of **all the drives** and make sure it has at least 10% free space:

```
check_drivesize "crit=free<10%" drive=*
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

To scan **all drives** but **require that specific drives are present** — going
CRITICAL if a mandatory drive is missing:

```
    check_drivesize drive=* require=D: require=E: "crit=free<10%"
CRITICAL: Required drive(s) not found: E: | OK: All drives ok
```

`require` (alias `mandatory-drives`) can be repeated and matches by drive letter
(with or without the trailing colon), volume label, or volume id. It is the one
`UsedPartitionSpace` feature gap that wildcard scanning alone could not cover:
`drive=*` silently reports OK when an expected disk has vanished, whereas
`require=` makes that a hard CRITICAL.

To check the size of all the drives and **display all values, not just problems**:

```
check_drivesize drive=* --show-all
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of all the drives and **return the value in gigabytes**. 
By default, units on performance data will be scaled to "something appropriate":

```
check_drivesize "perf-config=*(unit:g)"
L        cli CRITICAL: CRITICAL C:\\: 208.147GB/223.471GB used, D:\\: 399.607GB/465.759GB used
L        cli  Performance data: 'C:\ used'=0.00019g;0.00017;0.00019;0;0.00021 'C:\ used %'=93%;79;89;0;100 'D:\ used'=0.00038g;0.00035;0.00039;0;0.00044 'D:\ used %'=85%;79;89;0;100 'E:\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used %'=33%;79;89;0;100
```

To check the size of **a mounted volume** (c:\volume_test) and **make sure it has 1M free** space **warn if free space is less than 10M**:

```
   check_drivesize "crit=free<1M" "warn=free<10M" drive=c:\\volume_test
   C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;
```

To check the size of **all volumes** and **make sure they have 1M space free**:

```
check_drivesize "crit=free<1M" drive=all-volumes
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'E:\ free'=0B;0;0;0;0 'F:\ free'=0B;0;0;0;0
```

To check the size of **all fixed and network drives** and make sure they have at least 1gig free space:

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')"
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```


To check **all fixed and network drives but ignore C and F**:

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')" exclude=C:\\ exclude=D:\\
L     client OK: All drives ok
L     client  Performance data: 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

To **restrict by filesystem type** — for example, only NTFS volumes — use the
`filesystem` keyword (alias `fs`). The value compared against is whatever the
OS reports via `GetVolumeInformation`, typically uppercase: `NTFS`, `FAT32`,
`exFAT`, `ReFS`, `CDFS`, `UDF`. Empty string is reported for unmounted or
unreadable volumes.

```
check_drivesize drive=* "filter=fs = 'NTFS'"
L     client OK: All drives ok
L     client  Performance data: 'C:\ used'=205GB;...
```

Combine with `type` to scope further — for example, only **fixed** disks that
are **NTFS or ReFS**:

```
check_drivesize drive=* "filter=type = 'fixed' and fs in ('NTFS', 'ReFS')"
```

Use `like` for **case-insensitive** matching, since the OS reports uppercase
but a recipe written as `'ntfs'` should still work:

```
check_drivesize drive=* "filter=filesystem like 'ntfs'"
```

Drop volumes whose filesystem could not be read (e.g. an empty CD/DVD drive):

```
check_drivesize drive=* "filter=fs != ''"
```

Default via NRPE:

```
check_nrpe --host 192.168.56.103 --command check_drivesize
C:\: 205GB/223GB used, D:\: 448GB/466GB used, M:\: 2.6TB/2.68TB used|'C:\ used'=204GB;44;22;0;223 'C:\ used %'=91%;19;9;0;100 'D:\ used'=447GB;93;46;0;465...
```

**Check inode exhaustion (Linux) — a filesystem can be "not full" on bytes yet out of inodes:**

```
check_drivesize drive=/ "warn=inodes_used_pct > 85" "crit=inodes_used_pct > 95" "detail-syntax=${drive} inodes ${inodes_used}/${inodes_total} (${inodes_used_pct}%)"
OK: / inodes 350474/67108864 (1%)
```

The inode keywords are `inodes_total`, `inodes_free`, `inodes_used`,
`inodes_free_pct` and `inodes_used_pct`.



<a id="check_drivesize_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_drivesize_warn"></a>
    <a id="check_drivesize_crit"></a>
    <a id="check_drivesize_debug"></a>
    <a id="check_drivesize_show-all"></a>
    <a id="check_drivesize_escape-html"></a>
    <a id="check_drivesize_help"></a>
    <a id="check_drivesize_help-pb"></a>
    <a id="check_drivesize_show-default"></a>
    <a id="check_drivesize_help-short"></a>
    <a id="check_drivesize_magic"></a>
    <a id="check_drivesize_exclude"></a>
    <a id="check_drivesize_require"></a>
    <a id="check_drivesize_mandatory-drives"></a>

    | Option                                                  | Default Value                          | Description                                                                                                                                   |
    |---------------------------------------------------------|----------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_drivesize_filter)                       | mounted = 1                            | Filter which marks interesting items.                                                                                                         |
    | [warning](#check_drivesize_warning)                     | used > 80%                             | Filter which marks items which generates a warning state.                                                                                     |
    | warn                                                    |                                        | Short alias for warning                                                                                                                       |
    | [critical](#check_drivesize_critical)                   | used > 90%                             | Filter which marks items which generates a critical state.                                                                                    |
    | crit                                                    |                                        | Short alias for critical.                                                                                                                     |
    | [ok](#check_drivesize_ok)                               |                                        | Filter which marks items which generates an ok state.                                                                                         |
    | debug                                                   | N/A                                    | Show debugging information in the log                                                                                                         |
    | show-all                                                | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                              |
    | [empty-state](#check_drivesize_empty-state)             | unknown                                | Return status to use when nothing matched filter.                                                                                             |
    | [perf-config](#check_drivesize_perf-config)             |                                        | Performance data generation configuration                                                                                                     |
    | escape-html                                             | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                                                        |
    | help                                                    | N/A                                    | Show help screen (this screen)                                                                                                                |
    | help-pb                                                 | N/A                                    | Show help screen as a protocol buffer payload                                                                                                 |
    | show-default                                            | N/A                                    | Show default values for a given command                                                                                                       |
    | help-short                                              | N/A                                    | Show help screen (short format).                                                                                                              |
    | [top-syntax](#check_drivesize_top-syntax)               | ${status} ${problem_list}              | Top level syntax.                                                                                                                             |
    | [ok-syntax](#check_drivesize_ok-syntax)                 | %(status) All %(count) drive(s) are ok | ok syntax.                                                                                                                                    |
    | [empty-syntax](#check_drivesize_empty-syntax)           | %(status): No drives found             | Empty syntax.                                                                                                                                 |
    | [detail-syntax](#check_drivesize_detail-syntax)         | ${drive_or_name}: ${used}/${size} used | Detail level syntax.                                                                                                                          |
    | [perf-syntax](#check_drivesize_perf-syntax)             | ${drive_or_id}                         | Performance alias syntax.                                                                                                                     |
    | [drive](#check_drivesize_drive)                         |                                        | The drives to check.                                                                                                                          |
    | [ignore-unreadable](#check_drivesize_ignore-unreadable) | 1)] (=0                                | DEPRECATED (manually set filter instead) Ignore drives which are not reachable by the current user.                                           |
    | [mounted](#check_drivesize_mounted)                     | 1)] (=0                                | DEPRECATED (this is now default) Show only mounted rives i.e. drives which have a mount point.                                                |
    | magic                                                   |                                        | Magic number for use with scaling drive sizes.                                                                                                |
    | exclude                                                 |                                        | A list of drives not to check                                                                                                                 |
    | require                                                 |                                        | Drives that MUST be present: the check goes CRITICAL if any listed drive is not found, even when scanning wildcards. Alias: mandatory-drives. |
    | mandatory-drives                                        |                                        | Alias for require.                                                                                                                            |
    | [total](#check_drivesize_total)                         | 1)] (=0                                | Include the total of all matching drives                                                                                                      |



    <h5 id="check_drivesize_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `mounted = 1`

    <h5 id="check_drivesize_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `used > 80%`

    <h5 id="check_drivesize_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `used > 90%`

    <h5 id="check_drivesize_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_drivesize_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_drivesize_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_drivesize_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status} ${problem_list}`

    <h5 id="check_drivesize_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status) All %(count) drive(s) are ok`

    <h5 id="check_drivesize_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No drives found`

    <h5 id="check_drivesize_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${drive_or_name}: ${used}/${size} used`

    <h5 id="check_drivesize_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${drive_or_id}`

    <h5 id="check_drivesize_drive">drive:</h5>

    The drives to check.
    Multiple options can be used to check more then one drive or wildcards can be used to indicate multiple drives to check. Examples: drive=c, drive=d:, drive=*, drive=all-volumes, drive=all-drives


    <h5 id="check_drivesize_ignore-unreadable">ignore-unreadable:</h5>

    DEPRECATED (manually set filter instead) Ignore drives which are not reachable by the current user.
    For instance Microsoft Office creates a drive which cannot be read by normal users.

    *Default Value:* `1)] (=0`

    <h5 id="check_drivesize_mounted">mounted:</h5>

    DEPRECATED (this is now default) Show only mounted rives i.e. drives which have a mount point.

    *Default Value:* `1)] (=0`

    <h5 id="check_drivesize_total">total:</h5>

    Include the total of all matching drives

    *Default Value:* `1)] (=0`

=== "Linux"

    <a id="check_drivesize_warn"></a>
    <a id="check_drivesize_crit"></a>
    <a id="check_drivesize_debug"></a>
    <a id="check_drivesize_show-all"></a>
    <a id="check_drivesize_escape-html"></a>
    <a id="check_drivesize_help"></a>
    <a id="check_drivesize_help-pb"></a>
    <a id="check_drivesize_show-default"></a>
    <a id="check_drivesize_help-short"></a>
    <a id="check_drivesize_exclude"></a>

    | Option                                          | Default Value                          | Description                                                                                                      |
    |-------------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_drivesize_filter)               | mounted = 1                            | Filter which marks interesting items.                                                                            |
    | [warning](#check_drivesize_warning)             | used > 80%                             | Filter which marks items which generates a warning state.                                                        |
    | warn                                            |                                        | Short alias for warning                                                                                          |
    | [critical](#check_drivesize_critical)           | used > 90%                             | Filter which marks items which generates a critical state.                                                       |
    | crit                                            |                                        | Short alias for critical.                                                                                        |
    | [ok](#check_drivesize_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
    | debug                                           | N/A                                    | Show debugging information in the log                                                                            |
    | show-all                                        | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_drivesize_empty-state)     | unknown                                | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_drivesize_perf-config)     |                                        | Performance data generation configuration                                                                        |
    | escape-html                                     | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                            | N/A                                    | Show help screen (this screen)                                                                                   |
    | help-pb                                         | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                    | N/A                                    | Show default values for a given command                                                                          |
    | help-short                                      | N/A                                    | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_drivesize_top-syntax)       | ${status} ${problem_list}              | Top level syntax.                                                                                                |
    | [ok-syntax](#check_drivesize_ok-syntax)         | %(status) All %(count) drive(s) are ok | ok syntax.                                                                                                       |
    | [empty-syntax](#check_drivesize_empty-syntax)   | %(status): No drives found             | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_drivesize_detail-syntax) | ${drive_or_name}: ${used}/${size} used | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_drivesize_perf-syntax)     | ${drive_or_id}                         | Performance alias syntax.                                                                                        |
    | [drive](#check_drivesize_drive)                 |                                        | The drives to check.                                                                                             |
    | exclude                                         |                                        | A list of drives (mount points) not to check                                                                     |
    | [total](#check_drivesize_total)                 | 1)] (=0                                | Include the total of all matching drives                                                                         |



    <h5 id="check_drivesize_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `mounted = 1`

    <h5 id="check_drivesize_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `used > 80%`

    <h5 id="check_drivesize_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `used > 90%`

    <h5 id="check_drivesize_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_drivesize_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_drivesize_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_drivesize_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status} ${problem_list}`

    <h5 id="check_drivesize_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status) All %(count) drive(s) are ok`

    <h5 id="check_drivesize_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No drives found`

    <h5 id="check_drivesize_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${drive_or_name}: ${used}/${size} used`

    <h5 id="check_drivesize_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${drive_or_id}`

    <h5 id="check_drivesize_drive">drive:</h5>

    The drives to check.
    Multiple options can be used to check more than one mount or wildcards can be used to indicate multiple drives to check. Examples: drive=/, drive=/home, drive=*, drive=all-drives


    <h5 id="check_drivesize_total">total:</h5>

    Include the total of all matching drives

    *Default Value:* `1)] (=0`


<a id="check_drivesize_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option         | Description                                                           |
    |----------------|-----------------------------------------------------------------------|
    | drive          | Technical name of drive                                               |
    | drive_or_id    | Drive letter if present if not use id                                 |
    | drive_or_name  | Drive letter if present if not use name                               |
    | erasable       | 1 (true) if drive is erasable                                         |
    | filesystem     | Filesystem name as reported by the OS (e.g. NTFS, FAT32, exFAT, ReFS) |
    | flags          | String representation of flags                                        |
    | free           | Shorthand for total_free (Number of free bytes)                       |
    | free_pct       | Shorthand for total_free_pct (% free space)                           |
    | fs             | Shorthand alias for filesystem                                        |
    | hotplug        | 1 (true) if drive is hotplugable                                      |
    | id             | Drive or id of drive                                                  |
    | letter         | Letter the drive is mountedd on                                       |
    | media_type     | Get the media type                                                    |
    | mounted        | Check if a drive is mounted                                           |
    | name           | Descriptive name of drive                                             |
    | readable       | 1 (true) if drive is readable                                         |
    | removable      | 1 (true) if drive is removable                                        |
    | size           | Total size of drive                                                   |
    | total_free     | Number of free bytes                                                  |
    | total_free_pct | % free space                                                          |
    | total_used     | Number of used bytes                                                  |
    | total_used_pct | % used space                                                          |
    | type           | Type of drive                                                         |
    | used           | Number of used bytes                                                  |
    | used_pct       | Shorthand for total_used_pct (% used space)                           |
    | user_free      | Free space available to user (which runs NSClient++)                  |
    | user_free_pct  | % free space available to user                                        |
    | user_used      | Number of used bytes (related to user)                                |
    | user_used_pct  | % used space available to user                                        |
    | writable       | 1 (true) if drive is writable                                         |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

=== "Linux"

    | Option          | Description                                                        |
    |-----------------|--------------------------------------------------------------------|
    | drive           | Technical name of drive (mount point)                              |
    | drive_or_id     | Mount point if present if not use device                           |
    | drive_or_name   | Mount point if present if not use device                           |
    | erasable        | 1 (true) if drive is erasable                                      |
    | filesystem      | Filesystem type as reported by the OS (e.g. ext4, xfs, btrfs, nfs) |
    | flags           | String representation of flags                                     |
    | free            | Shorthand for total_free (Number of free bytes)                    |
    | free_pct        | Shorthand for total_free_pct (% free space)                        |
    | fs              | Shorthand alias for filesystem                                     |
    | hotplug         | 1 (true) if drive is hotplugable                                   |
    | id              | Drive or id of drive (device)                                      |
    | inodes_free     | Number of free inodes                                              |
    | inodes_free_pct | % free inodes                                                      |
    | inodes_total    | Total number of inodes on the filesystem                           |
    | inodes_used     | Number of used inodes                                              |
    | inodes_used_pct | % used inodes                                                      |
    | letter          | Letter the drive is mounted on (always empty on Unix)              |
    | media_type      | Get the media type                                                 |
    | mounted         | Check if a drive is mounted                                        |
    | name            | Descriptive name of drive (device)                                 |
    | readable        | 1 (true) if drive is readable                                      |
    | removable       | 1 (true) if drive is removable                                     |
    | size            | Total size of drive                                                |
    | total_free      | Number of free bytes                                               |
    | total_free_pct  | % free space                                                       |
    | total_used      | Number of used bytes                                               |
    | total_used_pct  | % used space                                                       |
    | type            | Type of drive                                                      |
    | used            | Number of used bytes                                               |
    | used_pct        | Shorthand for total_used_pct (% used space)                        |
    | user_free       | Free space available to user (which runs NSClient++)               |
    | user_free_pct   | % free space available to user                                     |
    | user_used       | Number of used bytes (related to user)                             |
    | user_used_pct   | % used space available to user                                     |
    | writable        | 1 (true) if drive is writable                                      |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

### check_files

Check various aspects of a file and/or folder.

**Jump to section:**

* [Sample Commands](#check_files_samples)
* [Command-line Arguments](#check_files_options)
* [Filter keywords](#check_files_filter_keys)


<a id="check_files_samples"></a>
#### Sample Commands

**Performance**

Order is somewhat important but mainly in the fact that some operations are more costly than others.
For instance line_count requires us to read and count the lines in each file so choosing between the following:
Fast version: `filter=creation < -2d and line_count > 100`

Slow version: `filter=line_count > 100 and creation < -2d`

The first one will be significantly faster if you have a thousand old files and 3 new ones.

On the other hand in this example `filter=creation < -2d and size > 100k` swapping them would not be noticeable.

**Checking versions of .exe files**

```
check_files path=c:/foo/ pattern=*.exe "filter=version != '1.0'" "detail-syntax=%(filename): %(version)" "warn=count > 1" show-all
L        cli WARNING: WARNING: 0/11 files (check_nrpe.exe: , nscp.exe: 0.5.0.16, reporter.exe: 0.5.0.16)
L        cli  Performance data: 'count'=11;1;0
```

**Using the line count with limited recursion:**

```
check_files path=c:/windows pattern=*.txt max-depth=1 "filter=line_count gt 100" "detail-syntax=%(filename): %(line_count)" "warn=count>0" show-all
L        cli WARNING: WARNING: 0/1 files (AsChkDev.txt: 328)
L        cli  Performance data: 'count'=1;0;0
```

**Check file sizes**

```
check_files path=c:/windows pattern=*.txt "detail-syntax=%(filename): %(size)" "warn=size>20k" max-depth=1
L        cli WARNING: WARNING: 1/6 files (AsChkDev.txt: 29738)
L        cli  Performance data: 'AsChkDev.txt size'=29.04101KB;20;0 'AsDCDVer.txt size'=0.02246KB;20;0 'AsHDIVer.txt size'=0.02734KB;20;0 'AsPEToolVer.txt size'=0.08789KB;20;0 'AsToolCDVer.txt size'=0.05273KB;20;0 'csup.txt size'=0.00976KB;20;0
```

**Report a file's checksum (keywords: `md5_checksum`, `sha1_checksum`, `sha256_checksum`, `sha384_checksum`, `sha512_checksum`):**

```
check_files path=/etc pattern=hostname "top-syntax=${list}" "detail-syntax=${filename}=${sha256_checksum}"
hostname=ec4e309d512b118e0ec6451c724b6dd9eaed955a9f1cb68b7d939765ac47af4d
```

**Alert if a file's checksum drifts from a known-good value (integrity monitoring):**

```
check_files path=/etc pattern=hostname "crit=md5_checksum != '63150f223f8488b21c374ae8ad13fb9c'"
OK: All 1 files are ok
```

Checksums are computed lazily — they are only calculated when a
`*_checksum` keyword is used in the filter or syntax.

**Folder aggregates on the `total` object (largest/average/smallest file, folder count):**

With `total`, an extra summary row aggregates the matched items. Beyond the
summed `size`, it now also exposes `smallest_size`, `largest_size`,
`average_size` and `folder_count`, so thresholds on the  largest or average 
file are expressible:

```
check_files path=c:/logs pattern=*.log total "filter=total = 0" "crit=total = 1 and largest_size > 100m" "detail-syntax=largest=${largest_size} avg=${average_size} folders=${folder_count}"
CRITICAL: largest=250M avg=12M folders=3
'total largest'=262144000B 'total average'=12582912B 'total folders'=3
```

These four keywords are meaningful on the `total` object (they aggregate across
everything `add`-ed into it); on an individual file row they read as 0.



<a id="check_files_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_files_warn"></a>
    <a id="check_files_crit"></a>
    <a id="check_files_debug"></a>
    <a id="check_files_show-all"></a>
    <a id="check_files_escape-html"></a>
    <a id="check_files_help"></a>
    <a id="check_files_help-pb"></a>
    <a id="check_files_show-default"></a>
    <a id="check_files_help-short"></a>
    <a id="check_files_file"></a>
    <a id="check_files_paths"></a>
    <a id="check_files_max-depth"></a>

    | Option                                      | Default Value                                                | Description                                                                                                      |
    |---------------------------------------------|--------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_files_filter)               |                                                              | Filter which marks interesting items.                                                                            |
    | [warning](#check_files_warning)             |                                                              | Filter which marks items which generates a warning state.                                                        |
    | warn                                        |                                                              | Short alias for warning                                                                                          |
    | [critical](#check_files_critical)           |                                                              | Filter which marks items which generates a critical state.                                                       |
    | crit                                        |                                                              | Short alias for critical.                                                                                        |
    | [ok](#check_files_ok)                       |                                                              | Filter which marks items which generates an ok state.                                                            |
    | debug                                       | N/A                                                          | Show debugging information in the log                                                                            |
    | show-all                                    | N/A                                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_files_empty-state)     | unknown                                                      | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_files_perf-config)     |                                                              | Performance data generation configuration                                                                        |
    | escape-html                                 | N/A                                                          | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                        | N/A                                                          | Show help screen (this screen)                                                                                   |
    | help-pb                                     | N/A                                                          | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                | N/A                                                          | Show default values for a given command                                                                          |
    | help-short                                  | N/A                                                          | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_files_top-syntax)       | ${status}: ${problem_count}/${count} files (${problem_list}) | Top level syntax.                                                                                                |
    | [ok-syntax](#check_files_ok-syntax)         | %(status): All %(count) files are ok                         | ok syntax.                                                                                                       |
    | [empty-syntax](#check_files_empty-syntax)   | No files found                                               | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_files_detail-syntax) | ${name}                                                      | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_files_perf-syntax)     | ${name}                                                      | Performance alias syntax.                                                                                        |
    | [path](#check_files_path)                   |                                                              | The path to search for files under.                                                                              |
    | file                                        |                                                              | Alias for path.                                                                                                  |
    | paths                                       |                                                              | A comma separated list of paths to scan                                                                          |
    | [pattern](#check_files_pattern)             | *.*                                                          | The pattern of files to search for (works like a filter but is faster and can be combined with a filter).        |
    | max-depth                                   |                                                              | Maximum depth to recurse                                                                                         |
    | [total](#check_files_total)                 | filter                                                       | Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter   |



    <h5 id="check_files_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_files_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_files_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_files_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_files_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_files_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_files_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${problem_count}/${count} files (${problem_list})`

    <h5 id="check_files_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All %(count) files are ok`

    <h5 id="check_files_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No files found`

    <h5 id="check_files_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}`

    <h5 id="check_files_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

    <h5 id="check_files_path">path:</h5>

    The path to search for files under.
    Notice that specifying multiple path will create an aggregate set you will not check each path individually.In other words if one path contains an error the entire check will result in error.


    <h5 id="check_files_pattern">pattern:</h5>

    The pattern of files to search for (works like a filter but is faster and can be combined with a filter).

    *Default Value:* `*.*`

    <h5 id="check_files_total">total:</h5>

    Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter

    *Default Value:* `filter`

=== "Linux"

    <a id="check_files_warn"></a>
    <a id="check_files_crit"></a>
    <a id="check_files_debug"></a>
    <a id="check_files_show-all"></a>
    <a id="check_files_escape-html"></a>
    <a id="check_files_help"></a>
    <a id="check_files_help-pb"></a>
    <a id="check_files_show-default"></a>
    <a id="check_files_help-short"></a>
    <a id="check_files_file"></a>
    <a id="check_files_paths"></a>
    <a id="check_files_max-depth"></a>

    | Option                                      | Default Value                                                | Description                                                                                                      |
    |---------------------------------------------|--------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_files_filter)               |                                                              | Filter which marks interesting items.                                                                            |
    | [warning](#check_files_warning)             |                                                              | Filter which marks items which generates a warning state.                                                        |
    | warn                                        |                                                              | Short alias for warning                                                                                          |
    | [critical](#check_files_critical)           |                                                              | Filter which marks items which generates a critical state.                                                       |
    | crit                                        |                                                              | Short alias for critical.                                                                                        |
    | [ok](#check_files_ok)                       |                                                              | Filter which marks items which generates an ok state.                                                            |
    | debug                                       | N/A                                                          | Show debugging information in the log                                                                            |
    | show-all                                    | N/A                                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_files_empty-state)     | unknown                                                      | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_files_perf-config)     |                                                              | Performance data generation configuration                                                                        |
    | escape-html                                 | N/A                                                          | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                        | N/A                                                          | Show help screen (this screen)                                                                                   |
    | help-pb                                     | N/A                                                          | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                | N/A                                                          | Show default values for a given command                                                                          |
    | help-short                                  | N/A                                                          | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_files_top-syntax)       | ${status}: ${problem_count}/${count} files (${problem_list}) | Top level syntax.                                                                                                |
    | [ok-syntax](#check_files_ok-syntax)         | %(status): All %(count) files are ok                         | ok syntax.                                                                                                       |
    | [empty-syntax](#check_files_empty-syntax)   | No files found                                               | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_files_detail-syntax) | ${name}                                                      | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_files_perf-syntax)     | ${name}                                                      | Performance alias syntax.                                                                                        |
    | [path](#check_files_path)                   |                                                              | The path to search for files under.                                                                              |
    | file                                        |                                                              | Alias for path.                                                                                                  |
    | paths                                       |                                                              | A comma separated list of paths to scan                                                                          |
    | [pattern](#check_files_pattern)             | *.*                                                          | The pattern of files to search for (works like a filter but is faster and can be combined with a filter).        |
    | max-depth                                   |                                                              | Maximum depth to recurse                                                                                         |
    | [total](#check_files_total)                 | filter                                                       | Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter   |



    <h5 id="check_files_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_files_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_files_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_files_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_files_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_files_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_files_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${problem_count}/${count} files (${problem_list})`

    <h5 id="check_files_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): All %(count) files are ok`

    <h5 id="check_files_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No files found`

    <h5 id="check_files_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}`

    <h5 id="check_files_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

    <h5 id="check_files_path">path:</h5>

    The path to search for files under.
    Notice that specifying multiple path will create an aggregate set you will not check each path individually.In other words if one path contains an error the entire check will result in error.


    <h5 id="check_files_pattern">pattern:</h5>

    The pattern of files to search for (works like a filter but is faster and can be combined with a filter).

    *Default Value:* `*.*`

    <h5 id="check_files_total">total:</h5>

    Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter

    *Default Value:* `filter`


<a id="check_files_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option          | Description                                                     |
    |-----------------|-----------------------------------------------------------------|
    | access          | Last access time                                                |
    | access_l        | Last access time (local time)                                   |
    | access_u        | Last access time (UTC)                                          |
    | age             | Seconds since file was last written                             |
    | average_size    | Average matched file size (aggregate; use on the total object)  |
    | creation        | When file was created                                           |
    | creation_l      | When file was created (local time)                              |
    | creation_u      | When file was created (UTC)                                     |
    | extension       | The filename extension                                          |
    | file            | The name of the file                                            |
    | filename        | The name of the file                                            |
    | folder_count    | Number of matched folders (aggregate; use on the total object)  |
    | largest_size    | Largest matched file size (aggregate; use on the total object)  |
    | line_count      | Number of lines in the file (text files)                        |
    | md5_checksum    | MD5 checksum of the file content (hex)                          |
    | name            | The name of the file                                            |
    | path            | Path of file                                                    |
    | sha1_checksum   | SHA-1 checksum of the file content (hex)                        |
    | sha256_checksum | SHA-256 checksum of the file content (hex)                      |
    | sha384_checksum | SHA-384 checksum of the file content (hex)                      |
    | sha512_checksum | SHA-512 checksum of the file content (hex)                      |
    | size            | File size                                                       |
    | smallest_size   | Smallest matched file size (aggregate; use on the total object) |
    | type            | Type of item (file or dir)                                      |
    | version         | Windows exe/dll file version (empty on Unix)                    |
    | write           | Alias for written                                               |
    | written         | When file was last written to                                   |
    | written_l       | When file was last written  to (local time)                     |
    | written_u       | When file was last written  to (UTC)                            |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

=== "Linux"

    | Option          | Description                                  |
    |-----------------|----------------------------------------------|
    | access          | Last access time                             |
    | access_l        | Last access time (local time)                |
    | access_u        | Last access time (UTC)                       |
    | age             | Seconds since file was last written          |
    | creation        | When file was created                        |
    | creation_l      | When file was created (local time)           |
    | creation_u      | When file was created (UTC)                  |
    | extension       | The filename extension                       |
    | file            | The name of the file                         |
    | filename        | The name of the file                         |
    | line_count      | Number of lines in the file (text files)     |
    | md5_checksum    | MD5 checksum of the file content (hex)       |
    | name            | The name of the file                         |
    | path            | Path of file                                 |
    | sha1_checksum   | SHA-1 checksum of the file content (hex)     |
    | sha256_checksum | SHA-256 checksum of the file content (hex)   |
    | sha384_checksum | SHA-384 checksum of the file content (hex)   |
    | sha512_checksum | SHA-512 checksum of the file content (hex)   |
    | size            | File size                                    |
    | type            | Type of item (file or dir)                   |
    | version         | Windows exe/dll file version (empty on Unix) |
    | write           | Alias for written                            |
    | written         | When file was last written to                |
    | written_l       | When file was last written  to (local time)  |
    | written_u       | When file was last written  to (UTC)         |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

### check_mount

Check that a filesystem is mounted with the expected fstype and options.

#### About `check_mount`

`check_mount` verifies that filesystems are mounted, and optionally that they
are mounted with the expected filesystem type and options. It reads the live
mount table (`/proc/self/mounts` via `getmntent`) so it reflects the actual
running state, not `/etc/fstab`. It is implemented on **Unix only**; on Windows
it reports that it is not supported.

Behaviour at a glance:

* With no `mount=` it inspects every *real* mount (pseudo-filesystems such as
  `proc`, `sysfs`, `cgroup`, `tmpfs` overlays … are skipped).
* With `mount=<path>` it inspects only that mount point, and reports
  **CRITICAL** `not mounted` when nothing is mounted there.
* `fstype=<type>` requires the mount to use that filesystem type; a mismatch is
  flagged as an `expected fstype differs` issue.
* `options=<a,b,c>` requires each listed mount option to be present; any missing
  option is flagged as a `missing options` issue.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                                             |
|--------------|--------------------------------------------------------|
| `mount`      | Path of the mounted folder                             |
| `device`     | Device backing this mount                              |
| `fstype`     | Filesystem type of this mount                          |
| `options`    | Mount options (comma separated)                        |
| `issues`     | Human-readable description of any problems found       |
| `has_issues` | `1` when this mount has one or more issues, else `0`   |

Default thresholds: **warning** `has_issues = 1`, **critical**
`issues like 'not mounted'`. So a missing filesystem is CRITICAL while a
fstype/options mismatch is WARNING out of the box; override `warning=` /
`critical=` to change that.

**Jump to section:**

* [Sample Commands](#check_mount_samples)
* [Command-line Arguments](#check_mount_options)
* [Filter keywords](#check_mount_filter_keys)


<a id="check_mount_samples"></a>
#### Sample Commands

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



<a id="check_mount_options"></a>
#### Command-line Arguments

<a id="check_mount_warn"></a>
<a id="check_mount_crit"></a>
<a id="check_mount_debug"></a>
<a id="check_mount_show-all"></a>
<a id="check_mount_escape-html"></a>
<a id="check_mount_help"></a>
<a id="check_mount_help-pb"></a>
<a id="check_mount_show-default"></a>
<a id="check_mount_help-short"></a>
<a id="check_mount_mount"></a>
<a id="check_mount_options"></a>
<a id="check_mount_fstype"></a>

| Option                                      | Default Value                                  | Description                                                                                                      |
|---------------------------------------------|------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_mount_filter)               |                                                | Filter which marks interesting items.                                                                            |
| [warning](#check_mount_warning)             | has_issues = 1                                 | Filter which marks items which generates a warning state.                                                        |
| warn                                        |                                                | Short alias for warning                                                                                          |
| [critical](#check_mount_critical)           | issues like 'not mounted'                      | Filter which marks items which generates a critical state.                                                       |
| crit                                        |                                                | Short alias for critical.                                                                                        |
| [ok](#check_mount_ok)                       |                                                | Filter which marks items which generates an ok state.                                                            |
| debug                                       | N/A                                            | Show debugging information in the log                                                                            |
| show-all                                    | N/A                                            | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_mount_empty-state)     | unknown                                        | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_mount_perf-config)     |                                                | Performance data generation configuration                                                                        |
| escape-html                                 | N/A                                            | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                        | N/A                                            | Show help screen (this screen)                                                                                   |
| help-pb                                     | N/A                                            | Show help screen as a protocol buffer payload                                                                    |
| show-default                                | N/A                                            | Show default values for a given command                                                                          |
| help-short                                  | N/A                                            | Show help screen (short format).                                                                                 |
| [top-syntax](#check_mount_top-syntax)       | ${status}: ${problem_list}                     | Top level syntax.                                                                                                |
| [ok-syntax](#check_mount_ok-syntax)         | %(status): mounts are as expected              | ok syntax.                                                                                                       |
| [empty-syntax](#check_mount_empty-syntax)   | check_mount found nothing matching this filter | Empty syntax.                                                                                                    |
| [detail-syntax](#check_mount_detail-syntax) | mount ${mount} ${issues}                       | Detail level syntax.                                                                                             |
| [perf-syntax](#check_mount_perf-syntax)     | ${mount}                                       | Performance alias syntax.                                                                                        |
| mount                                       |                                                | The mount point to check (omit to check all real mounts)                                                         |
| options                                     |                                                | The mount options to expect (comma separated)                                                                    |
| fstype                                      |                                                | The filesystem type to expect                                                                                    |



<h5 id="check_mount_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_mount_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `has_issues = 1`

<h5 id="check_mount_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `issues like 'not mounted'`

<h5 id="check_mount_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_mount_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_mount_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_mount_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_mount_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): mounts are as expected`

<h5 id="check_mount_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `check_mount found nothing matching this filter`

<h5 id="check_mount_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `mount ${mount} ${issues}`

<h5 id="check_mount_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${mount}`


<a id="check_mount_filter_keys"></a>
#### Filter keywords

| Option     | Description                                        |
|------------|----------------------------------------------------|
| device     | Device backing this mount                          |
| fstype     | Filesystem type of this mount                      |
| has_issues | 1 when any issue was found, else 0                 |
| issues     | Issues found (empty when the mount is as expected) |
| mount      | Path of the mounted folder                         |
| options    | Mount options                                      |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

### check_shadowcopy

*Available on Windows only.*

Check VSS shadow-copy (Volume Shadow Copy) recency, count and shadow-storage usage per volume (Windows).

#### About `check_shadowcopy`

`check_shadowcopy` verifies **Volume Shadow Copy Service (VSS)** snapshots — the
data behind "Previous Versions", scheduled restore points and many backup
products. It answers "does each volume still have a *recent* shadow copy, and is
shadow storage healthy?", which is often the first sign that a backup / snapshot
job has silently stopped running.

It reads `Win32_ShadowCopy` (one row per snapshot) and groups it by volume, then
joins per-volume usage from `Win32_ShadowStorage`. One row is produced per volume
that has at least one shadow copy.

Keywords:

| Keyword       | Description                                                                        |
|---------------|------------------------------------------------------------------------------------|
| `volume`      | The volume the shadow copies belong to (VolumeName device path)                    |
| `count`       | Number of shadow copies on this volume                                             |
| `newest`      | **Seconds** since the newest shadow copy (-1 if the date is unknown)               |
| `newest_date` | Timestamp of the newest shadow copy (UTC)                                          |
| `used`        | Shadow storage currently used on this volume, in bytes                             |
| `allocated`   | Shadow storage currently allocated, in bytes                                       |
| `max_size`    | Shadow storage maximum, in bytes (0 when unbounded / not resolved)                 |
| `used_pct`    | Percentage of the shadow-storage maximum in use (0 when `max_size` is unbounded)   |

`newest` is seconds, so threshold it with durations: `newest > 26h`, `newest > 2d`.

Defaults: **WARNING** when `newest > 26h`, **CRITICAL** when `newest > 50h` — i.e.
tuned for roughly **daily** snapshots; loosen them (`newest > 8d`) for weekly
schedules. The empty state is **OK**: a volume with no shadow copies is common
and not inherently a problem. If snapshots are *required*, pass
`empty-state=critical` so their absence is alerted.

**Caveats:** shadow copies are transient (VSS deletes the oldest when storage
fills), so a shrinking `count` or rising `used_pct` is an early warning that
older restore points are being aged out. `max_size` is 0 when shadow storage is
configured as "unbounded", which makes `used_pct` inert by design.

**Jump to section:**

* [Sample Commands](#check_shadowcopy_samples)
* [Command-line Arguments](#check_shadowcopy_options)
* [Filter keywords](#check_shadowcopy_filter_keys)


<a id="check_shadowcopy_samples"></a>
#### Sample Commands

**Default check on a volume with recent snapshots:**

```
check_shadowcopy
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```

**Default check on a host with no shadow copies (empty state is OK):**

```
check_shadowcopy
OK: No shadow copies found
```

**Require snapshots to exist — alert when there are none:**

```
check_shadowcopy empty-state=critical
CRITICAL: No shadow copies found
```

**Alert when the newest snapshot is stale (weekly schedule):**

```
check_shadowcopy "warning=newest > 8d" "critical=newest > 15d"
OK: \\?\Volume{4c2b...}\: 4 copies, newest 2026-07-09 02:00:01 UTC
```

**Alert when shadow storage is nearly full (oldest restore points about to age out):**

```
check_shadowcopy "warning=used_pct > 80" "critical=used_pct > 95"
WARNING: \\?\Volume{4c2b...}\: 20 copies, newest 2026-07-11 07:00:03 UTC
```

**Require at least a minimum number of restore points per volume:**

```
check_shadowcopy "critical=count < 3"
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```

**Custom output with counts and storage usage:**

```
check_shadowcopy "top-syntax=%(status): %(list)" "detail-syntax=%(volume): %(count) copies, %(used) of %(max_size) used (%(used_pct)%)"
OK: \\?\Volume{4c2b...}\: 12 copies, 1610612736 of 10737418240 used (15%)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_shadowcopy --argument "warning=newest > 26h"
OK: \\?\Volume{4c2b...}\: 12 copies, newest 2026-07-11 07:00:03 UTC
```



<a id="check_shadowcopy_options"></a>
#### Command-line Arguments

<a id="check_shadowcopy_warn"></a>
<a id="check_shadowcopy_crit"></a>
<a id="check_shadowcopy_debug"></a>
<a id="check_shadowcopy_show-all"></a>
<a id="check_shadowcopy_escape-html"></a>
<a id="check_shadowcopy_help"></a>
<a id="check_shadowcopy_help-pb"></a>
<a id="check_shadowcopy_show-default"></a>
<a id="check_shadowcopy_help-short"></a>

| Option                                           | Default Value                                                | Description                                                                                                      |
|--------------------------------------------------|--------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_shadowcopy_filter)               |                                                              | Filter which marks interesting items.                                                                            |
| [warning](#check_shadowcopy_warning)             | newest > 26h                                                 | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                                              | Short alias for warning                                                                                          |
| [critical](#check_shadowcopy_critical)           | newest > 50h                                                 | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                                              | Short alias for critical.                                                                                        |
| [ok](#check_shadowcopy_ok)                       |                                                              | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                                          | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_shadowcopy_empty-state)     | ok                                                           | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_shadowcopy_perf-config)     |                                                              | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                                          | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                                          | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                                          | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                                          | Show default values for a given command                                                                          |
| help-short                                       | N/A                                                          | Show help screen (short format).                                                                                 |
| [top-syntax](#check_shadowcopy_top-syntax)       | ${status}: ${list}                                           | Top level syntax.                                                                                                |
| [ok-syntax](#check_shadowcopy_ok-syntax)         | %(status): All %(count) volume(s) have recent shadow copies. | ok syntax.                                                                                                       |
| [empty-syntax](#check_shadowcopy_empty-syntax)   | %(status): No shadow copies found                            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_shadowcopy_detail-syntax) | ${volume}: ${count} copies, newest ${newest_date}            | Detail level syntax.                                                                                             |
| [perf-syntax](#check_shadowcopy_perf-syntax)     | ${volume}                                                    | Performance alias syntax.                                                                                        |



<h5 id="check_shadowcopy_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_shadowcopy_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `newest > 26h`

<h5 id="check_shadowcopy_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `newest > 50h`

<h5 id="check_shadowcopy_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_shadowcopy_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_shadowcopy_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_shadowcopy_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_shadowcopy_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All %(count) volume(s) have recent shadow copies.`

<h5 id="check_shadowcopy_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No shadow copies found`

<h5 id="check_shadowcopy_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${volume}: ${count} copies, newest ${newest_date}`

<h5 id="check_shadowcopy_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${volume}`


<a id="check_shadowcopy_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                                                       |
|-------------|---------------------------------------------------------------------------------------------------|
| allocated   | Shadow storage currently allocated in bytes                                                       |
| max_size    | Shadow storage maximum in bytes (0 if unbounded/unresolved)                                       |
| newest      | Seconds since the newest shadow copy (-1 if unknown); threshold with durations, e.g. newest > 26h |
| newest_date | Timestamp of the newest shadow copy on this volume (UTC)                                          |
| used        | Shadow storage used on this volume in bytes                                                       |
| used_pct    | Percentage of the shadow-storage maximum in use                                                   |
| volume      | Volume the shadow copies belong to (VolumeName device path)                                       |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

### check_share

*Available on Windows only.*

Check Windows SMB shares: list them, or verify that specific required shares exist (Windows).

#### About `check_share`

`check_share` inspects the host's **SMB shares** (Windows `Win32_Share`). It has
two modes:

- **List mode** (no `share=`): enumerate every share on the host — useful for
  inventory or `show-all` output.
- **Required mode** (one or more `share=<name>`): verify that specific shares
  exist. Each requested share becomes a row with an `exists` flag, and the check
  is **CRITICAL** when a required share is missing (default `crit=not exists`).

This complements [`check_uncpath`](CheckDisk_check_uncpath_samples.md), which
checks a *remote* share's free space, with the server-side "are my shares
published?" view.

Keywords (one row per share):

| Keyword       | Description                                                      |
|---------------|------------------------------------------------------------------|
| `name`        | Share name (e.g. `C$`, `Public`)                                 |
| `path`        | Local path the share maps to (empty for `IPC$`)                  |
| `description` | Share description / comment                                      |
| `type`        | Share kind: `disk`, `printer`, `device`, `ipc` or `unknown`      |
| `is_admin`    | `1` for an administrative share (`C$`, `ADMIN$`, `IPC$`)         |
| `exists`      | `1` if the share exists; `0` for a requested-but-missing share   |

Defaults: `crit=not exists` (inert in list mode, since every listed share
exists), empty-state **OK** (a host with no shares is not inherently a problem).
Windows share names are case-insensitive, so `share=public` matches a `Public`
share.

**Jump to section:**

* [Sample Commands](#check_share_samples)
* [Command-line Arguments](#check_share_options)
* [Filter keywords](#check_share_filter_keys)


<a id="check_share_samples"></a>
#### Sample Commands

**List all shares on the host:**

```
check_share
OK: All 3 share(s) ok.|'count'=3
```

**Verify a required share exists (present → OK):**

```
check_share share=C$
OK: C$ (type=disk, path=C:\, exists=1)|'count'=1
```

**Verify a required share exists (missing → CRITICAL):**

```
check_share share=Public
CRITICAL: Public (type=disk, path=, exists=0)|'count'=1
```

**Require several shares at once:**

```
check_share share=Public share=Profiles share=Software
OK: All 3 share(s) ok.|'count'=3
```

**Alert if any non-administrative share is unexpectedly published:**

```
check_share "crit=is_admin = 0" "top-syntax=%(status): %(problem_list)" "detail-syntax=%(name) -> %(path)"
OK: All 5 share(s) ok.
```

**List only non-admin (user-created) shares with their paths:**

```
check_share "filter=is_admin = 0" "top-syntax=%(status): %(list)" "detail-syntax=%(name) (%(type)) -> %(path)"
OK: Public (disk) -> C:\Shared, Profiles (disk) -> D:\Profiles
```

**Over NRPE against a file server:**

```
check_nscp_client --host 192.168.56.103 --command check_share --argument "share=Public" --argument "share=Profiles"
OK: All 2 share(s) ok.
```



<a id="check_share_options"></a>
#### Command-line Arguments

<a id="check_share_warn"></a>
<a id="check_share_crit"></a>
<a id="check_share_debug"></a>
<a id="check_share_show-all"></a>
<a id="check_share_escape-html"></a>
<a id="check_share_help"></a>
<a id="check_share_help-pb"></a>
<a id="check_share_show-default"></a>
<a id="check_share_help-short"></a>
<a id="check_share_share"></a>

| Option                                      | Default Value                                          | Description                                                                                                                                           |
|---------------------------------------------|--------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_share_filter)               |                                                        | Filter which marks interesting items.                                                                                                                 |
| [warning](#check_share_warning)             |                                                        | Filter which marks items which generates a warning state.                                                                                             |
| warn                                        |                                                        | Short alias for warning                                                                                                                               |
| [critical](#check_share_critical)           | not exists                                             | Filter which marks items which generates a critical state.                                                                                            |
| crit                                        |                                                        | Short alias for critical.                                                                                                                             |
| [ok](#check_share_ok)                       |                                                        | Filter which marks items which generates an ok state.                                                                                                 |
| debug                                       | N/A                                                    | Show debugging information in the log                                                                                                                 |
| show-all                                    | N/A                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                      |
| [empty-state](#check_share_empty-state)     | ok                                                     | Return status to use when nothing matched filter.                                                                                                     |
| [perf-config](#check_share_perf-config)     |                                                        | Performance data generation configuration                                                                                                             |
| escape-html                                 | N/A                                                    | Escape any < and > characters to prevent HTML encoding                                                                                                |
| help                                        | N/A                                                    | Show help screen (this screen)                                                                                                                        |
| help-pb                                     | N/A                                                    | Show help screen as a protocol buffer payload                                                                                                         |
| show-default                                | N/A                                                    | Show default values for a given command                                                                                                               |
| help-short                                  | N/A                                                    | Show help screen (short format).                                                                                                                      |
| [top-syntax](#check_share_top-syntax)       | ${status}: ${list}                                     | Top level syntax.                                                                                                                                     |
| [ok-syntax](#check_share_ok-syntax)         | %(status): All %(count) share(s) ok.                   | ok syntax.                                                                                                                                            |
| [empty-syntax](#check_share_empty-syntax)   | %(status): No shares found                             | Empty syntax.                                                                                                                                         |
| [detail-syntax](#check_share_detail-syntax) | ${name} (type=${type}, path=${path}, exists=${exists}) | Detail level syntax.                                                                                                                                  |
| [perf-syntax](#check_share_perf-syntax)     | ${name}                                                | Performance alias syntax.                                                                                                                             |
| share                                       |                                                        | Require a specific share to exist (repeatable). The check is CRITICAL when a requested share is missing. When omitted, all shares are listed instead. |



<h5 id="check_share_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_share_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_share_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `not exists`

<h5 id="check_share_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_share_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_share_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_share_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_share_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All %(count) share(s) ok.`

<h5 id="check_share_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No shares found`

<h5 id="check_share_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name} (type=${type}, path=${path}, exists=${exists})`

<h5 id="check_share_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_share_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                 |
|-------------|-------------------------------------------------------------|
| description | Share description / comment                                 |
| exists      | 1 if the share exists (0 for a requested-but-missing share) |
| is_admin    | 1 for an administrative share (C$, ADMIN$, IPC$)            |
| name        | Share name (e.g. C$, Public)                                |
| path        | Local path the share maps to (empty for IPC$)               |
| type        | Share kind: disk, printer, device, ipc or unknown           |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

### check_single_file

Check various aspects of a single file (size, age, line count, version, ...). Simpler alternative to check_files when you only need to inspect one specific file.

#### About `check_single_file`

`check_single_file` is a focused variant of [`check_files`](#check_files)
for inspecting a single, known path. There is no `path` + `pattern` scan and
no recursion — you point it at one file and apply a filter / threshold to its
attributes (`size`, `age`, `version`, `line_count`, …).

Behaviour at a glance:

* If `file=` (or its alias `path=`) is missing → **UNKNOWN** with
  `No file specified (use file=<path>)`.
* If the file does not exist (or the path points at a directory) →
  **UNKNOWN** with `File not found: <path>`.
* Otherwise the single file is fed to the filter and `warn` / `crit`
  decide the status. With no thresholds the result is **OK** confirming
  the file exists.

**Jump to section:**

* [Sample Commands](#check_single_file_samples)
* [Command-line Arguments](#check_single_file_options)
* [Filter keywords](#check_single_file_filter_keys)


<a id="check_single_file_samples"></a>
#### Sample Commands

**Confirm a file exists (no thresholds)**

```
check_single_file file=C:/Windows/System32/notepad.exe
L        cli OK: notepad.exe (size=201728, age=12345)
```

**Warn when a log file grows too large**

```
check_single_file file=C:/logs/app.log "warn=size > 10M" "crit=size > 100M"
L        cli OK: app.log (size=524288, age=42)
```

**Warn when a file becomes stale (age in seconds)**

```
check_single_file file=C:/windows/WindowsUpdate.log "warn=age > 5m" "crit=age > 1h"
L        cli CRITICAL: WindowsUpdate.log (size=276, age=917)
```

**Check a specific binary's version**

```
check_single_file file="C:/Windows/System32/notepad.exe" "crit=version != '1.2.3.4'" "detail-syntax=%(filename): %(version)"
L        cli CRITICAL: notepad.exe: 6.2.26100.8115
```

**Custom output formatting**

The same `top-syntax` / `detail-syntax` / `ok-syntax` keys as `check_files`
are accepted. Because there is exactly one item, `%(list)` in the top
template expands to the detail line for that single file:

```
check_single_file file=C:/windows/WindowsUpdate.log "warn=size > 1M" "top-syntax=%(status) %(list)" "detail-syntax=%(filename) is %(size) bytes, last written %(written)"
L        cli OK: OK WindowsUpdate.log is 276 bytes, last written 2026-04-30 11:42:36
```

**`path=` works as an alias for `file=`**

This makes it easy to migrate command lines from `check_files`:

```
check_single_file path=C:/Windows/win.ini
L        cli OK: win.ini (size=92, age=873123)
```




<a id="check_single_file_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_single_file_warn"></a>
    <a id="check_single_file_crit"></a>
    <a id="check_single_file_debug"></a>
    <a id="check_single_file_show-all"></a>
    <a id="check_single_file_escape-html"></a>
    <a id="check_single_file_help"></a>
    <a id="check_single_file_help-pb"></a>
    <a id="check_single_file_show-default"></a>
    <a id="check_single_file_help-short"></a>
    <a id="check_single_file_file"></a>
    <a id="check_single_file_path"></a>

    | Option                                            | Default Value                          | Description                                                                                                      |
    |---------------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_single_file_filter)               |                                        | Filter which marks interesting items.                                                                            |
    | [warning](#check_single_file_warning)             |                                        | Filter which marks items which generates a warning state.                                                        |
    | warn                                              |                                        | Short alias for warning                                                                                          |
    | [critical](#check_single_file_critical)           |                                        | Filter which marks items which generates a critical state.                                                       |
    | crit                                              |                                        | Short alias for critical.                                                                                        |
    | [ok](#check_single_file_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
    | debug                                             | N/A                                    | Show debugging information in the log                                                                            |
    | show-all                                          | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_single_file_empty-state)     | ok                                     | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_single_file_perf-config)     |                                        | Performance data generation configuration                                                                        |
    | escape-html                                       | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                              | N/A                                    | Show help screen (this screen)                                                                                   |
    | help-pb                                           | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                      | N/A                                    | Show default values for a given command                                                                          |
    | help-short                                        | N/A                                    | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_single_file_top-syntax)       | %(status): %(list)                     | Top level syntax.                                                                                                |
    | [ok-syntax](#check_single_file_ok-syntax)         | %(status): %(filename) is ok           | ok syntax.                                                                                                       |
    | [empty-syntax](#check_single_file_empty-syntax)   | No file inspected                      | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_single_file_detail-syntax) | %(filename) (size=%(size), age=%(age)) | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_single_file_perf-syntax)     | %(filename)                            | Performance alias syntax.                                                                                        |
    | file                                              |                                        | The file to check.                                                                                               |
    | path                                              |                                        | Alias for file.                                                                                                  |



    <h5 id="check_single_file_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_single_file_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_single_file_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_single_file_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_single_file_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_single_file_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_single_file_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `%(status): %(list)`

    <h5 id="check_single_file_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): %(filename) is ok`

    <h5 id="check_single_file_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No file inspected`

    <h5 id="check_single_file_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `%(filename) (size=%(size), age=%(age))`

    <h5 id="check_single_file_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `%(filename)`

=== "Linux"

    <a id="check_single_file_warn"></a>
    <a id="check_single_file_crit"></a>
    <a id="check_single_file_debug"></a>
    <a id="check_single_file_show-all"></a>
    <a id="check_single_file_escape-html"></a>
    <a id="check_single_file_help"></a>
    <a id="check_single_file_help-pb"></a>
    <a id="check_single_file_show-default"></a>
    <a id="check_single_file_help-short"></a>
    <a id="check_single_file_file"></a>
    <a id="check_single_file_path"></a>

    | Option                                            | Default Value                          | Description                                                                                                      |
    |---------------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_single_file_filter)               |                                        | Filter which marks interesting items.                                                                            |
    | [warning](#check_single_file_warning)             |                                        | Filter which marks items which generates a warning state.                                                        |
    | warn                                              |                                        | Short alias for warning                                                                                          |
    | [critical](#check_single_file_critical)           |                                        | Filter which marks items which generates a critical state.                                                       |
    | crit                                              |                                        | Short alias for critical.                                                                                        |
    | [ok](#check_single_file_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
    | debug                                             | N/A                                    | Show debugging information in the log                                                                            |
    | show-all                                          | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_single_file_empty-state)     | ok                                     | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_single_file_perf-config)     |                                        | Performance data generation configuration                                                                        |
    | escape-html                                       | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                              | N/A                                    | Show help screen (this screen)                                                                                   |
    | help-pb                                           | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                      | N/A                                    | Show default values for a given command                                                                          |
    | help-short                                        | N/A                                    | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_single_file_top-syntax)       | %(status): %(list)                     | Top level syntax.                                                                                                |
    | [ok-syntax](#check_single_file_ok-syntax)         | %(status): %(filename) is ok           | ok syntax.                                                                                                       |
    | [empty-syntax](#check_single_file_empty-syntax)   | No file inspected                      | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_single_file_detail-syntax) | %(filename) (size=%(size), age=%(age)) | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_single_file_perf-syntax)     | %(filename)                            | Performance alias syntax.                                                                                        |
    | file                                              |                                        | The file to check.                                                                                               |
    | path                                              |                                        | Alias for file.                                                                                                  |



    <h5 id="check_single_file_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_single_file_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_single_file_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_single_file_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_single_file_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_single_file_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_single_file_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `%(status): %(list)`

    <h5 id="check_single_file_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): %(filename) is ok`

    <h5 id="check_single_file_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No file inspected`

    <h5 id="check_single_file_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `%(filename) (size=%(size), age=%(age))`

    <h5 id="check_single_file_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `%(filename)`


<a id="check_single_file_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option          | Description                                                     |
    |-----------------|-----------------------------------------------------------------|
    | access          | Last access time                                                |
    | access_l        | Last access time (local time)                                   |
    | access_u        | Last access time (UTC)                                          |
    | age             | Seconds since file was last written                             |
    | average_size    | Average matched file size (aggregate; use on the total object)  |
    | creation        | When file was created                                           |
    | creation_l      | When file was created (local time)                              |
    | creation_u      | When file was created (UTC)                                     |
    | extension       | The filename extension                                          |
    | file            | The name of the file                                            |
    | filename        | The name of the file                                            |
    | folder_count    | Number of matched folders (aggregate; use on the total object)  |
    | largest_size    | Largest matched file size (aggregate; use on the total object)  |
    | line_count      | Number of lines in the file (text files)                        |
    | md5_checksum    | MD5 checksum of the file content (hex)                          |
    | name            | The name of the file                                            |
    | path            | Path of file                                                    |
    | sha1_checksum   | SHA-1 checksum of the file content (hex)                        |
    | sha256_checksum | SHA-256 checksum of the file content (hex)                      |
    | sha384_checksum | SHA-384 checksum of the file content (hex)                      |
    | sha512_checksum | SHA-512 checksum of the file content (hex)                      |
    | size            | File size                                                       |
    | smallest_size   | Smallest matched file size (aggregate; use on the total object) |
    | type            | Type of item (file or dir)                                      |
    | version         | Windows exe/dll file version (empty on Unix)                    |
    | write           | Alias for written                                               |
    | written         | When file was last written to                                   |
    | written_l       | When file was last written  to (local time)                     |
    | written_u       | When file was last written  to (UTC)                            |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

=== "Linux"

    | Option          | Description                                  |
    |-----------------|----------------------------------------------|
    | access          | Last access time                             |
    | access_l        | Last access time (local time)                |
    | access_u        | Last access time (UTC)                       |
    | age             | Seconds since file was last written          |
    | creation        | When file was created                        |
    | creation_l      | When file was created (local time)           |
    | creation_u      | When file was created (UTC)                  |
    | extension       | The filename extension                       |
    | file            | The name of the file                         |
    | filename        | The name of the file                         |
    | line_count      | Number of lines in the file (text files)     |
    | md5_checksum    | MD5 checksum of the file content (hex)       |
    | name            | The name of the file                         |
    | path            | Path of file                                 |
    | sha1_checksum   | SHA-1 checksum of the file content (hex)     |
    | sha256_checksum | SHA-256 checksum of the file content (hex)   |
    | sha384_checksum | SHA-384 checksum of the file content (hex)   |
    | sha512_checksum | SHA-512 checksum of the file content (hex)   |
    | size            | File size                                    |
    | type            | Type of item (file or dir)                   |
    | version         | Windows exe/dll file version (empty on Unix) |
    | write           | Alias for written                            |
    | written         | When file was last written to                |
    | written_l       | When file was last written  to (local time)  |
    | written_u       | When file was last written  to (UTC)         |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

### check_storagepool

*Available on Windows only.*

Check Storage Spaces pool health and capacity (Windows).

Checks the health and capacity of Windows Storage Spaces pools, read from
`MSFT_StoragePool` in the `root\Microsoft\Windows\Storage` WMI namespace. The
primordial pool (the reservoir of unpooled physical disks) is excluded, so only
real Storage Spaces are reported.

| Keyword                 | Description                                                       |
|-------------------------|-------------------------------------------------------------------|
| `name`                  | Pool friendly name.                                               |
| `health_status`         | `Healthy`, `Warning`, `Unhealthy` or `Unknown`.                   |
| `operational_status`    | Synthesised single value: `OK`, `ReadOnly`, or the health string. |
| `capacity`              | Total pool capacity in bytes (perf).                              |
| `used`                  | Allocated (used) space in bytes (perf).                           |
| `free`                  | Unallocated space in bytes (perf).                                |
| `free_pct` / `used_pct` | Percentage free / used (perf).                                    |
| `is_readonly`           | `1` if the pool is read-only.                                     |

Defaults: WARNING on `Warning` health or `< 20%` free; CRITICAL on `Unhealthy`
health or `< 10%` free. If the Storage namespace/class is unavailable (no Storage
Spaces, older Windows) the check reports no pools and returns OK — it never fails
just because the feature is absent. This is a natural companion to the
physical-disk device state exposed by `check_disk_health`.

**Jump to section:**

* [Sample Commands](#check_storagepool_samples)
* [Command-line Arguments](#check_storagepool_options)
* [Filter keywords](#check_storagepool_filter_keys)


<a id="check_storagepool_samples"></a>
#### Sample Commands

**Check Storage Spaces pool health and capacity (Windows):**

```
check_storagepool
OK: All storage pools are healthy.
'Pool1 free_pct'=64%;20;10 'Pool1 capacity'=8.0T;; 'Pool1 used'=2.9T;;
```

By default the check is WARNING when a pool reports `Warning` health or drops
below 20% free, and CRITICAL when a pool is `Unhealthy` or below 10% free. A
system with no Storage Spaces pools returns OK.

**Alert only on pool health, ignoring capacity:**

```
check_storagepool "warn=health_status = 'Warning'" "crit=health_status = 'Unhealthy' or is_readonly = 1" "detail-syntax=${name}: ${health_status} (${operational_status})"
CRITICAL: Data: Unhealthy (Unhealthy)
```

Keywords: `name`, `health_status` (Healthy/Warning/Unhealthy/Unknown),
`operational_status`, `capacity`, `used`, `free`, `free_pct`, `used_pct`,
`is_readonly`.



<a id="check_storagepool_options"></a>
#### Command-line Arguments

<a id="check_storagepool_warn"></a>
<a id="check_storagepool_crit"></a>
<a id="check_storagepool_debug"></a>
<a id="check_storagepool_show-all"></a>
<a id="check_storagepool_escape-html"></a>
<a id="check_storagepool_help"></a>
<a id="check_storagepool_help-pb"></a>
<a id="check_storagepool_show-default"></a>
<a id="check_storagepool_help-short"></a>

| Option                                            | Default Value                                       | Description                                                                                                      |
|---------------------------------------------------|-----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_storagepool_filter)               |                                                     | Filter which marks interesting items.                                                                            |
| [warning](#check_storagepool_warning)             | health_status = 'Warning' or free_pct < 20          | Filter which marks items which generates a warning state.                                                        |
| warn                                              |                                                     | Short alias for warning                                                                                          |
| [critical](#check_storagepool_critical)           | health_status = 'Unhealthy' or free_pct < 10        | Filter which marks items which generates a critical state.                                                       |
| crit                                              |                                                     | Short alias for critical.                                                                                        |
| [ok](#check_storagepool_ok)                       |                                                     | Filter which marks items which generates an ok state.                                                            |
| debug                                             | N/A                                                 | Show debugging information in the log                                                                            |
| show-all                                          | N/A                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_storagepool_empty-state)     | ok                                                  | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_storagepool_perf-config)     |                                                     | Performance data generation configuration                                                                        |
| escape-html                                       | N/A                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                              | N/A                                                 | Show help screen (this screen)                                                                                   |
| help-pb                                           | N/A                                                 | Show help screen as a protocol buffer payload                                                                    |
| show-default                                      | N/A                                                 | Show default values for a given command                                                                          |
| help-short                                        | N/A                                                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_storagepool_top-syntax)       | ${status}: ${list}                                  | Top level syntax.                                                                                                |
| [ok-syntax](#check_storagepool_ok-syntax)         | %(status): All storage pools are healthy.           | ok syntax.                                                                                                       |
| [empty-syntax](#check_storagepool_empty-syntax)   | %(status): No storage pools found                   | Empty syntax.                                                                                                    |
| [detail-syntax](#check_storagepool_detail-syntax) | ${name}: ${health_status}, ${used}/${capacity} used | Detail level syntax.                                                                                             |
| [perf-syntax](#check_storagepool_perf-syntax)     | ${name}                                             | Performance alias syntax.                                                                                        |



<h5 id="check_storagepool_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_storagepool_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `health_status = 'Warning' or free_pct < 20`

<h5 id="check_storagepool_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `health_status = 'Unhealthy' or free_pct < 10`

<h5 id="check_storagepool_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_storagepool_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_storagepool_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_storagepool_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_storagepool_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All storage pools are healthy.`

<h5 id="check_storagepool_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No storage pools found`

<h5 id="check_storagepool_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name}: ${health_status}, ${used}/${capacity} used`

<h5 id="check_storagepool_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_storagepool_filter_keys"></a>
#### Filter keywords

| Option             | Description                                         |
|--------------------|-----------------------------------------------------|
| capacity           | Total pool capacity in bytes                        |
| free               | Unallocated (free) pool space in bytes              |
| free_pct           | Percentage of free pool space                       |
| health_status      | Pool health: Healthy, Warning, Unhealthy or Unknown |
| is_readonly        | 1 if the pool is read-only                          |
| name               | Storage pool friendly name                          |
| operational_status | Pool operational status: OK, ReadOnly, ...          |
| used               | Allocated (used) pool space in bytes                |
| used_pct           | Percentage of used pool space                       |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

### check_uncpath

*Available on Windows only.*

Check free space on a UNC path (server share), with optional alternate credentials.

Checks free space on a UNC path (`\\server\share`), optionally authenticating
with alternate credentials. This fills a gap `check_drivesize` cannot: it only
sees OS-mounted drives and cannot take an arbitrary UNC path or supply
credentials.

| Keyword                 | Description                                                                  |
|-------------------------|------------------------------------------------------------------------------|
| `path`                  | The UNC path being checked.                                                  |
| `size`                  | Total size of the share in bytes (perf).                                     |
| `free`                  | Free space on the share in bytes (perf).                                     |
| `used`                  | Used space in bytes (perf).                                                  |
| `user_free`             | Free space available to the querying user, honouring per-user quotas (perf). |
| `free_pct` / `used_pct` | Percentage free / used (perf).                                               |

Options: `path=` (repeatable), `user=`, `password=`. Defaults mirror
`check_drivesize` (`used_pct > 80` warning, `> 90` critical). On Windows the free
space comes from `GetDiskFreeSpaceEx`, with an optional `WNetAddConnection2` for
alternate credentials that is disconnected after the query. On non-Windows the
path must already be mounted (alternate-credential UNC access is Windows-only).

**Jump to section:**

* [Sample Commands](#check_uncpath_samples)
* [Command-line Arguments](#check_uncpath_options)
* [Filter keywords](#check_uncpath_filter_keys)


<a id="check_uncpath_samples"></a>
#### Sample Commands

**Check free space on a UNC path:**

```
check_uncpath path=\\fileserver\data "crit=used_pct > 90"
OK: \\fileserver\data: 1.2T/2.0T used (800G free)
'\\fileserver\data used_pct'=60%;80;90 '\\fileserver\data free'=800G;;
```

Unlike `check_drivesize` (which only sees OS-mounted drives), `check_uncpath`
takes an arbitrary `\\server\share` and reports quota-aware free space.

**With alternate credentials (Windows):**

```
check_uncpath path=\\fileserver\backups user=DOMAIN\svc password=secret "crit=free < 100g"
OK: \\fileserver\backups: 400G/2.0T used (1.6T free)
```

`user`/`password` map to a temporary `WNetAddConnection2` before the query and
are disconnected afterwards. `user_free` reports the free space available to the
querying account (honouring per-user quotas) as distinct from the share's total
`free`.

**Multiple paths:**

```
check_uncpath path=\\a\share path=\\b\share "crit=free_pct < 10" "top-syntax=${status}: ${problem_list}"
```



<a id="check_uncpath_options"></a>
#### Command-line Arguments

<a id="check_uncpath_warn"></a>
<a id="check_uncpath_crit"></a>
<a id="check_uncpath_debug"></a>
<a id="check_uncpath_show-all"></a>
<a id="check_uncpath_escape-html"></a>
<a id="check_uncpath_help"></a>
<a id="check_uncpath_help-pb"></a>
<a id="check_uncpath_show-default"></a>
<a id="check_uncpath_help-short"></a>
<a id="check_uncpath_path"></a>
<a id="check_uncpath_user"></a>
<a id="check_uncpath_password"></a>

| Option                                        | Default Value                                | Description                                                                                                      |
|-----------------------------------------------|----------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_uncpath_filter)               |                                              | Filter which marks interesting items.                                                                            |
| [warning](#check_uncpath_warning)             | used_pct > 80                                | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                              | Short alias for warning                                                                                          |
| [critical](#check_uncpath_critical)           | used_pct > 90                                | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                              | Short alias for critical.                                                                                        |
| [ok](#check_uncpath_ok)                       |                                              | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                          | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_uncpath_empty-state)     | unknown                                      | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_uncpath_perf-config)     |                                              | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                          | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                          | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                          | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                          | Show default values for a given command                                                                          |
| help-short                                    | N/A                                          | Show help screen (short format).                                                                                 |
| [top-syntax](#check_uncpath_top-syntax)       | ${status}: ${list}                           | Top level syntax.                                                                                                |
| [ok-syntax](#check_uncpath_ok-syntax)         | %(status): All UNC paths are ok.             | ok syntax.                                                                                                       |
| [empty-syntax](#check_uncpath_empty-syntax)   | %(status): No paths checked                  | Empty syntax.                                                                                                    |
| [detail-syntax](#check_uncpath_detail-syntax) | ${path}: ${used}/${size} used (${free} free) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_uncpath_perf-syntax)     | ${path}                                      | Performance alias syntax.                                                                                        |
| path                                          |                                              | The UNC path(s) to check, e.g. \\server\share. Repeat for multiple.                                              |
| user                                          |                                              | Optional user name for alternate credentials (Windows).                                                          |
| password                                      |                                              | Optional password for alternate credentials (Windows).                                                           |



<h5 id="check_uncpath_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_uncpath_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `used_pct > 80`

<h5 id="check_uncpath_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `used_pct > 90`

<h5 id="check_uncpath_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_uncpath_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_uncpath_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_uncpath_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_uncpath_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All UNC paths are ok.`

<h5 id="check_uncpath_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No paths checked`

<h5 id="check_uncpath_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${path}: ${used}/${size} used (${free} free)`

<h5 id="check_uncpath_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${path}`


<a id="check_uncpath_filter_keys"></a>
#### Filter keywords

| Option    | Description                                                      |
|-----------|------------------------------------------------------------------|
| free      | Free space on the share in bytes                                 |
| free_pct  | Percentage of free space                                         |
| path      | The UNC path being checked                                       |
| size      | Total size of the share in bytes                                 |
| used      | Used space on the share in bytes                                 |
| used_pct  | Percentage of used space                                         |
| user_free | Free space available to the querying user (quota-aware) in bytes |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

## Configuration

| Path / Section                    | Description |
|-----------------------------------|-------------|
| [/settings/disk](#/settings/disk) |             |


### /settings/disk <a id="/settings/disk"></a>



| Key                                  | Default Value | Description              |
|--------------------------------------|---------------|--------------------------|
| [disable](#disable-automatic-checks) |               | Disable automatic checks |


```ini
# 
[/settings/disk]
```

#### Disable automatic checks <a id="/settings/disk/disable"></a>

A comma separated list of checks to disable in the collector: disk_io, disk_free. Please note disabling these will mean part of NSClient++ will no longer function as expected.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/disk](#/settings/disk)   |
| Key:           | disable                             |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/disk]
# Disable automatic checks
disable=
```
