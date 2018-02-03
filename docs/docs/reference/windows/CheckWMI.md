# CheckWMI

Check status via WMI




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

* [Samples](#check_wmi_samples)

* [Command-line Arguments](#check_wmi_options)
* [Filter keywords](#check_wmi_filter_keys)


<a name="check_wmi_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckWMI_check_wmi_samples.md)_

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




<a name="check_wmi_warn"/>
<a name="check_wmi_crit"/>
<a name="check_wmi_debug"/>
<a name="check_wmi_show-all"/>
<a name="check_wmi_escape-html"/>
<a name="check_wmi_help"/>
<a name="check_wmi_help-pb"/>
<a name="check_wmi_show-default"/>
<a name="check_wmi_help-short"/>
<a name="check_wmi_target"/>
<a name="check_wmi_user"/>
<a name="check_wmi_password"/>
<a name="check_wmi_query"/>
<a name="check_wmi_options"/>
#### Command-line Arguments


| Option                                    | Default Value | Description                                                                                                      |
|-------------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_wmi_filter)               |               | Filter which marks interesting items.                                                                            |
| [warning](#check_wmi_warning)             |               | Filter which marks items which generates a warning state.                                                        |
| warn                                      |               | Short alias for warning                                                                                          |
| [critical](#check_wmi_critical)           |               | Filter which marks items which generates a critical state.                                                       |
| crit                                      |               | Short alias for critical.                                                                                        |
| [ok](#check_wmi_ok)                       |               | Filter which marks items which generates an ok state.                                                            |
| debug                                     | N/A           | Show debugging information in the log                                                                            |
| show-all                                  | N/A           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_wmi_empty-state)     | ignored       | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_wmi_perf-config)     |               | Performance data generation configuration                                                                        |
| escape-html                               | N/A           | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                      | N/A           | Show help screen (this screen)                                                                                   |
| help-pb                                   | N/A           | Show help screen as a protocol buffer payload                                                                    |
| show-default                              | N/A           | Show default values for a given command                                                                          |
| help-short                                | N/A           | Show help screen (short format).                                                                                 |
| [top-syntax](#check_wmi_top-syntax)       | ${list}       | Top level syntax.                                                                                                |
| [ok-syntax](#check_wmi_ok-syntax)         |               | ok syntax.                                                                                                       |
| [empty-syntax](#check_wmi_empty-syntax)   |               | Empty syntax.                                                                                                    |
| [detail-syntax](#check_wmi_detail-syntax) | %(line)       | Detail level syntax.                                                                                             |
| [perf-syntax](#check_wmi_perf-syntax)     |               | Performance alias syntax.                                                                                        |
| target                                    |               | The target to check (for checking remote machines).                                                              |
| user                                      |               | Remote username when checking remote machines.                                                                   |
| password                                  |               | Remote password when checking remote machines.                                                                   |
| [namespace](#check_wmi_namespace)         | root\cimv2    | The WMI root namespace to bind to.                                                                               |
| query                                     |               | The WMI query to execute.                                                                                        |



<h5 id="check_wmi_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_wmi_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_wmi_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_wmi_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_wmi_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_wmi_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_wmi_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${list}`

<h5 id="check_wmi_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_wmi_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_wmi_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `%(line)`

<h5 id="check_wmi_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.


<h5 id="check_wmi_namespace">namespace:</h5>

The WMI root namespace to bind to.

*Default Value:* `root\cimv2`


<a name="check_wmi_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |




## Configuration



| Path / Section                            | Description         |
|-------------------------------------------|---------------------|
| [/settings/targets](#target-list-section) | TARGET LIST SECTION |



### TARGET LIST SECTION <a id="/settings/targets"/>

A list of available remote target systems


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






