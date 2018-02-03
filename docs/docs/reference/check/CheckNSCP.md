# CheckNSCP

Use this module to check the healt and status of NSClient++ it self




## Queries

A quick reference for all available queries (check commands) in the CheckNSCP module.

**List of commands:**

A list of all available queries (check commands)

| Command                                   | Description                                    |
|-------------------------------------------|------------------------------------------------|
| [check_nscp](#check_nscp)                 | Check the internal healt of NSClient++.        |
| [check_nscp_version](#check_nscp_version) | Check the version of NSClient++ which is used. |




### check_nscp

Check the internal healt of NSClient++.


* [Command-line Arguments](#check_nscp_options)





<a name="check_nscp_help"/>
<a name="check_nscp_help-pb"/>
<a name="check_nscp_show-default"/>
<a name="check_nscp_help-short"/>
<a name="check_nscp_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_nscp_version

Check the version of NSClient++ which is used.


* [Command-line Arguments](#check_nscp_version_options)
* [Filter keywords](#check_nscp_version_filter_keys)





<a name="check_nscp_version_warn"/>
<a name="check_nscp_version_crit"/>
<a name="check_nscp_version_debug"/>
<a name="check_nscp_version_show-all"/>
<a name="check_nscp_version_escape-html"/>
<a name="check_nscp_version_help"/>
<a name="check_nscp_version_help-pb"/>
<a name="check_nscp_version_show-default"/>
<a name="check_nscp_version_help-short"/>
<a name="check_nscp_version_options"/>
#### Command-line Arguments


| Option                                             | Default Value                                   | Description                                                                                                      |
|----------------------------------------------------|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_nscp_version_filter)               |                                                 | Filter which marks interesting items.                                                                            |
| [warning](#check_nscp_version_warning)             |                                                 | Filter which marks items which generates a warning state.                                                        |
| warn                                               |                                                 | Short alias for warning                                                                                          |
| [critical](#check_nscp_version_critical)           |                                                 | Filter which marks items which generates a critical state.                                                       |
| crit                                               |                                                 | Short alias for critical.                                                                                        |
| [ok](#check_nscp_version_ok)                       |                                                 | Filter which marks items which generates an ok state.                                                            |
| debug                                              | N/A                                             | Show debugging information in the log                                                                            |
| show-all                                           | N/A                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_nscp_version_empty-state)     | ignored                                         | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_nscp_version_perf-config)     |                                                 | Performance data generation configuration                                                                        |
| escape-html                                        | N/A                                             | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                               | N/A                                             | Show help screen (this screen)                                                                                   |
| help-pb                                            | N/A                                             | Show help screen as a protocol buffer payload                                                                    |
| show-default                                       | N/A                                             | Show default values for a given command                                                                          |
| help-short                                         | N/A                                             | Show help screen (short format).                                                                                 |
| [top-syntax](#check_nscp_version_top-syntax)       | ${status}: ${list}                              | Top level syntax.                                                                                                |
| [ok-syntax](#check_nscp_version_ok-syntax)         |                                                 | ok syntax.                                                                                                       |
| [empty-syntax](#check_nscp_version_empty-syntax)   |                                                 | Empty syntax.                                                                                                    |
| [detail-syntax](#check_nscp_version_detail-syntax) | ${release}.${major}.${minor}.${build} (${date}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_nscp_version_perf-syntax)     | version                                         | Performance alias syntax.                                                                                        |



<h5 id="check_nscp_version_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_nscp_version_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_nscp_version_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_nscp_version_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_nscp_version_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_nscp_version_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_nscp_version_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_nscp_version_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_nscp_version_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_nscp_version_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${release}.${major}.${minor}.${build} (${date})`

<h5 id="check_nscp_version_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `version`


<a name="check_nscp_version_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| build         | The build (the 3 in 0.1.2.3)                                                                                 |
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| date          | The NSClient++ Build date                                                                                    |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| major         | The major (the 1 in 0.1.2.3)                                                                                 |
| minor         | The minor (the 2 in 0.1.2.3)                                                                                 |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| release       | The release (the 0 in 0.1.2.3)                                                                               |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| version       | The NSClient++ Version as a string                                                                           |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |




