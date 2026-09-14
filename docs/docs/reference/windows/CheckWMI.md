# CheckWMI

*Available on Windows only.*

Check status via WMI

## Enable module

To enable this module and allow using the commands you need to add `CheckWMI = enabled` to the `[/modules]` section in nsclient.ini:

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

| Option                            | Default Value | Description                                         |
|-----------------------------------|---------------|-----------------------------------------------------|
| target                            |               | The target to check (for checking remote machines). |
| user                              |               | Remote username when checking remote machines.      |
| password                          |               | Remote password when checking remote machines.      |
| [namespace](#check_wmi_namespace) | root\cimv2    | The WMI root namespace to bind to.                  |
| [query](#check_wmi_query)         |               | The WMI query to execute.                           |



<h5 id="check_wmi_namespace">namespace:</h5>

The WMI root namespace to bind to.
While 'query access' in [/settings/wmi] is restricted this must match 'allowed namespaces', and may not be changed at all when that list is empty.

*Default Value:* `root\cimv2`

<h5 id="check_wmi_query">query:</h5>

The WMI query to execute.
Which queries may be run here is governed by 'query access' in [/settings/wmi]: by default any query is run, but an operator can restrict this to queries reading an allowed class, or to names predefined in [/settings/wmi/queries], in which case this takes such a name.



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

| Path / Section                                   | Description            |
|--------------------------------------------------|------------------------|
| [/settings/wmi](#/settings/wmi)                  |                        |
| [/settings/wmi/queries](#predefined-wmi-queries) | PREDEFINED WMI QUERIES |
| [/settings/wmi/targets](#target-list-section)    | TARGET LIST SECTION    |


### /settings/wmi <a id="/settings/wmi"></a>



| Key                                           | Default Value | Description            |
|-----------------------------------------------|---------------|------------------------|
| [allowed classes](#allowed-wmi-classes)       |               | ALLOWED WMI CLASSES    |
| [allowed namespaces](#allowed-wmi-namespaces) |               | ALLOWED WMI NAMESPACES |
| [query access](#wmi-query-access-mode)        | any           | WMI QUERY ACCESS MODE  |


```ini
# 
[/settings/wmi]
query access=any
```

#### ALLOWED WMI CLASSES <a id="/settings/wmi/allowed classes"></a>

Comma separated list of WMI classes check_wmi may read when 'query access' is set to allowed. Entries may contain * and ?, for example Win32_Service, Win32_PerfFormattedData_*.
Only a plain 'SELECT ... FROM <class> [WHERE ...]' can be checked this way. Anything else - ASSOCIATORS OF, REFERENCES OF, a class path carrying a namespace - is refused rather than guessed at, and has to be configured as a predefined query instead.


| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/wmi](#/settings/wmi) |
| Key:           | allowed classes                 |
| Default value: | _N/A_                           |


**Sample:**

```
[/settings/wmi]
# ALLOWED WMI CLASSES
allowed classes=
```

#### ALLOWED WMI NAMESPACES <a id="/settings/wmi/allowed namespaces"></a>

Comma separated list of WMI namespaces check_wmi may bind to when 'query access' is not any. Entries may contain * and ?.
Leaving this empty means the caller may not change the namespace at all: only the default root\\cimv2 is used. It has no effect in the default any mode.


| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/wmi](#/settings/wmi) |
| Key:           | allowed namespaces              |
| Default value: | _N/A_                           |


**Sample:**

```
[/settings/wmi]
# ALLOWED WMI NAMESPACES
allowed namespaces=
```

#### WMI QUERY ACCESS MODE <a id="/settings/wmi/query access"></a>

Which WMI queries a caller may ask check_wmi to run: any (the default - any query the caller sends, which is how every earlier release behaved), allowed (only a plain SELECT whose class matches 'allowed classes') or predefined (only names defined in the [/settings/wmi/queries] section).
WMI reaches most of what the machine knows, including the filesystem through Win32_Directory and CIM_DataFile, so on a host where callers may pass arguments (NRPE with 'allow arguments', or the REST API) this decides how much of it a check can read. See the 'Restricting what a check may read' section of the documentation.


| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/wmi](#/settings/wmi) |
| Key:           | query access                    |
| Default value: | `any`                           |


**Sample:**

```
[/settings/wmi]
# WMI QUERY ACCESS MODE
query access=any
```

### PREDEFINED WMI QUERIES <a id="/settings/wmi/queries"></a>

WMI queries check_wmi may run by name, as <name> = <query>.
A name defined here can be used as query=<name> in any access mode, and is the only thing accepted when 'query access' is set to predefined. The query is not parsed: an operator who writes it here has vouched for it.



```ini
# WMI queries check_wmi may run by name, as <name> = <query>.
[/settings/wmi/queries]
```

### TARGET LIST SECTION <a id="/settings/wmi/targets"></a>

A list of available remote target systems


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





