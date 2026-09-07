# CheckWMI

*Available on Windows only.*

Check status via WMI

## Enable module

To enable this module and and allow using the commands you need to ass `CheckWMI = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckWMI = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckWMI module.

**List of commands:**

A list of all available queries (check commands)

| Command                 | Description                                                            |
|-------------------------|------------------------------------------------------------------------|
| [check_wmi](#check_wmi) | Check a set of WMI values and return rows which are matching criteria. |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command  | Description                   |
|----------|-------------------------------|
| checkwmi | Alias for: :query:`check_wmi` |

### check_wmi

Check a set of WMI values and return rows which are matching criteria.

#### About `check_wmi`

`check_wmi` runs an arbitrary WQL query and turns each returned row into a
filter record. It is the escape hatch for everything the purpose-built checks do
not cover: if the data is in WMI, this exposes it to the same
`filter=` / `warning=` / `critical=` / `top-syntax=` machinery as every other
check.

##### Keywords are the query's own columns

Unlike every other check, the keyword vocabulary is **not fixed** — it is
derived from the columns your `query=` selects. A query of
`SELECT Name, FreeSpace FROM Win32_LogicalDisk` gives you `Name` and
`FreeSpace` as keywords, and each is also emitted as performance data.

Because those keywords only exist once a query has been run, the generated
"Filter keywords" section below can only list the generic summary keywords —
none of this check's own. There is one more that is always available and is not
listed there either: **`line`**, the whole row rendered as a comma-separated
string, which is what the default `detail-syntax` (`%(line)`) uses.

This also means `SELECT *` is usually the wrong thing to write: it produces a
keyword and a performance-data series per column of the class, most of which you
do not want graphed. Select the columns you intend to use.

##### Remote hosts

`target=` names a host to query instead of the local machine, resolved through
the module's configured targets when one matches and treated as a hostname
otherwise; `user=` and `password=` supply alternate credentials.
`namespace=` binds to a WMI root other than the default `root\cimv2` —
`root\wmi` for driver/ACPI classes, `root\Microsoft\Windows\Storage` for Storage
Spaces, and so on.

Remote WMI needs DCOM/RPC reachable through the firewall and an account with
remote-WMI rights on the target. Where the data is available locally, running
the check on the monitored host is both faster and simpler to secure.

##### Caveats

There are no default thresholds, and the default `empty-state` is `ignored`, so
a query that matches nothing returns OK. If "no rows" is itself the alarm
condition — a service class that should always have an instance — set
`empty-state=critical` explicitly.

A WMI query is comparatively expensive and can block for seconds on a busy or
unhealthy host; keep queries narrow and consider wrapping slow ones in
[`check_timeout`](../check/CheckHelpers.md#check_timeout).

**Jump to section:**

* [Sample Commands](#check_wmi_samples)
* [Command-line Arguments](#check_wmi_options)
* [Filter keywords](#check_wmi_filter_keys)


<a id="check_wmi_samples"></a>
#### Sample Commands

Basic check to see/fetch information (no check)::

```
check_wmi "query=Select Version,Caption from win32_OperatingSystem"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

A simple string check::

```
check_wmi "query=Select Version,Caption from win32_OperatingSystem" "warn=Version not like '6.3'" "crit=Version not like '6'"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

Simple check via **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_wmi -a "query=Select Version,Caption from win32_OperatingSystem" "warn=Version not like '6.3'" "crit=Version not like '6'"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

A simple integer (number) check::

```
check_wmi "query=Select BuildNumber from win32_OperatingSystem" "warn=BuildNumber < 9600" "crit=BuildNumber < 8000"
L        cli OK: 9600
L        cli  Performance data: 'BuildNumber'=9600;9600;8000
```

Using performance options to customize the performance data::

```
check_wmi "query=select Name, AvgDiskQueueLength from Win32_PerfFormattedData_PerfDisk_PhysicalDisk" "warn=AvgDiskQueueLength>0" "perf-syntax=%(Name)" "perf-config=*(prefix:'time')"
L        cli OK: 0, _Total, 0, 0 C:, 0, 1 D:
L        cli  Performance data: 'time_Total'=0;0;0 'time0 C:'=0;0;0 'time1 D:'=0;0;0
```

Adding values to the message::

```
check_wmi "query=Select BuildNumber from win32_OperatingSystem" "warn=BuildNumber < 9600" "crit=BuildNumber < 8000" "detail-syntax=You have build %(BuildNumber)" show-all
L        cli OK: You have build 10240
L        cli  Performance data: 'BuildNumber'=10240;9600;8000
```




<a id="check_wmi_options"></a>
#### Command-line Arguments

<a id="check_wmi_target"></a>
<a id="check_wmi_user"></a>
<a id="check_wmi_password"></a>
<a id="check_wmi_query"></a>

| Option                            | Default Value | Description                                         |
|-----------------------------------|---------------|-----------------------------------------------------|
| target                            |               | The target to check (for checking remote machines). |
| user                              |               | Remote username when checking remote machines.      |
| password                          |               | Remote password when checking remote machines.      |
| [namespace](#check_wmi_namespace) | root\cimv2    | The WMI root namespace to bind to.                  |
| query                             |               | The WMI query to execute.                           |



<h5 id="check_wmi_namespace">namespace:</h5>

The WMI root namespace to bind to.

*Default Value:* `root\cimv2`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value |
|-----------------------------------------------------------------------------------------------------------|---------------|
| <a id="check_wmi_filter"></a>[filter](../common-options.md#filter)                                        |               |
| <a id="check_wmi_warning"></a>[warning](../common-options.md#warning)                                     |               |
| <a id="check_wmi_warn"></a>[warn](../common-options.md#warn)                                              |               |
| <a id="check_wmi_critical"></a>[critical](../common-options.md#critical)                                  |               |
| <a id="check_wmi_crit"></a>[crit](../common-options.md#crit)                                              |               |
| <a id="check_wmi_ok"></a>[ok](../common-options.md#ok)                                                    |               |
| <a id="check_wmi_debug"></a>[debug](../common-options.md#debug)                                           | false         |
| <a id="check_wmi_show-all"></a>[show-all](../common-options.md#show-all)                                  | false         |
| <a id="check_wmi_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored       |
| <a id="check_wmi_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |               |
| <a id="check_wmi_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false         |
| <a id="check_wmi_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,             |
| <a id="check_wmi_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${list}       |
| <a id="check_wmi_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |               |
| <a id="check_wmi_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |               |
| <a id="check_wmi_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | %(line)       |
| <a id="check_wmi_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         |               |
| <a id="check_wmi_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |               |
| <a id="check_wmi_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |               |
| <a id="check_wmi_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1            |
| <a id="check_wmi_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_wmi_filter_keys"></a>
#### Filter keywords

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                | Description         |
|-----------------------------------------------|---------------------|
| [/settings/wmi/targets](#target-list-section) | TARGET LIST SECTION |


### TARGET LIST SECTION <a id="/settings/wmi/targets"></a>

A list of available remote target systems


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





