# CheckWMI

Check status via WMI



## List of commands

A list of all available queries (check commands)

| Command                 | Description                                                            |
|-------------------------|------------------------------------------------------------------------|
| [check_wmi](#check_wmi) | Check a set of WMI values and return rows which are matching criteria. |


## List of command aliases

A list of all short hand aliases for queries (check commands)


| Command  | Description                   |
|----------|-------------------------------|
| checkwmi | Alias for: :query:`check_wmi` |


## List of Configuration







# Queries

A quick reference for all available queries (check commands) in the CheckWMI module.

## check_wmi

Check a set of WMI values and return rows which are matching criteria.

### Usage

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


### Usage


| Option                                    | Default Value | Description                                                                                                      |
|-------------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_wmi_filter)               |               | Filter which marks interesting items.                                                                            |
| [warning](#check_wmi_warning)             |               | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_wmi_warn)                   |               | Short alias for warning                                                                                          |
| [critical](#check_wmi_critical)           |               | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_wmi_crit)                   |               | Short alias for critical.                                                                                        |
| [ok](#check_wmi_ok)                       |               | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_wmi_debug)                 | N/A           | Show debugging information in the log                                                                            |
| [show-all](#check_wmi_show-all)           | N/A           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_wmi_empty-state)     | ignored       | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_wmi_perf-config)     |               | Performance data generation configuration                                                                        |
| [escape-html](#check_wmi_escape-html)     | N/A           | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_wmi_help)                   | N/A           | Show help screen (this screen)                                                                                   |
| [help-pb](#check_wmi_help-pb)             | N/A           | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_wmi_show-default)   | N/A           | Show default values for a given command                                                                          |
| [help-short](#check_wmi_help-short)       | N/A           | Show help screen (short format).                                                                                 |
| [top-syntax](#check_wmi_top-syntax)       | ${list}       | Top level syntax.                                                                                                |
| [ok-syntax](#check_wmi_ok-syntax)         |               | ok syntax.                                                                                                       |
| [empty-syntax](#check_wmi_empty-syntax)   |               | Empty syntax.                                                                                                    |
| [detail-syntax](#check_wmi_detail-syntax) | %(line)       | Detail level syntax.                                                                                             |
| [perf-syntax](#check_wmi_perf-syntax)     |               | Performance alias syntax.                                                                                        |
| [target](#check_wmi_target)               |               | The target to check (for checking remote machines).                                                              |
| [user](#check_wmi_user)                   |               | Remote username when checking remote machines.                                                                   |
| [password](#check_wmi_password)           |               | Remote password when checking remote machines.                                                                   |
| [namespace](#check_wmi_namespace)         | root\cimv2    | The WMI root namespace to bind to.                                                                               |
| [query](#check_wmi_query)                 |               | The WMI query to execute.                                                                                        |


<a name="check_wmi_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |







<a name="check_wmi_warning"/>
### warning



**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |







<a name="check_wmi_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_wmi_critical"/>
### critical



**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |







<a name="check_wmi_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_wmi_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |







<a name="check_wmi_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_wmi_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_wmi_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_wmi_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_wmi_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_wmi_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_wmi_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_wmi_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_wmi_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_wmi_top-syntax"/>
### top-syntax


**Deafult Value:** ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_wmi_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_wmi_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_wmi_detail-syntax"/>
### detail-syntax


**Deafult Value:** %(line)

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key | Value |
|-----|-------|






<a name="check_wmi_perf-syntax"/>
### perf-syntax



**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key | Value |
|-----|-------|






<a name="check_wmi_target"/>
### target



**Description:**
The target to check (for checking remote machines).

<a name="check_wmi_user"/>
### user



**Description:**
Remote username when checking remote machines.

<a name="check_wmi_password"/>
### password



**Description:**
Remote password when checking remote machines.

<a name="check_wmi_namespace"/>
### namespace


**Deafult Value:** root\cimv2

**Description:**
The WMI root namespace to bind to.

<a name="check_wmi_query"/>
### query



**Description:**
The WMI query to execute.



# Configuration

<a name="/settings/targets"/>
## TARGET LIST SECTION

A list of available remote target systems

```ini
# A list of available remote target systems
[/settings/targets]

```






