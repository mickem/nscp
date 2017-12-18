# CheckNSCP

Use this module to check the health and status of NSClient++ it self



**List of commands:**

A list of all available queries (check commands)

| Command                                   | Description                                    |
|-------------------------------------------|------------------------------------------------|
| [check_nscp](#check_nscp)                 | Check the internal health of NSClient++.        |
| [check_nscp_version](#check_nscp_version) | Check the version of NSClient++ which is used. |







## Queries

A quick reference for all available queries (check commands) in the CheckNSCP module.

### check_nscp

Check the internal health of NSClient++.


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



<a name="check_nscp_version_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_nscp_version_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.






<a name="check_nscp_version_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.






<a name="check_nscp_version_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_nscp_version_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Default Value:* | `ignored`



<a name="check_nscp_version_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_nscp_version_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).


*Default Value:* | `${status}: ${list}`



<a name="check_nscp_version_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).





<a name="check_nscp_version_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.





<a name="check_nscp_version_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).


*Default Value:* | `${release}.${major}.${minor}.${build} (${date})`



<a name="check_nscp_version_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Default Value:* | `version`




<a name="check_nscp_version_filter_keys"/>
#### Filter keywords


| Option                                             | Description                                                                                                  |
|----------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [build](#check_nscp_version_build)                 | The build (the 3 in 0.1.2.3)                                                                                 |
| [count](#check_nscp_version_count)                 | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_nscp_version_crit_count)       | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_nscp_version_crit_list)         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [date](#check_nscp_version_date)                   | The NSClient++ Build date                                                                                    |
| [detail_list](#check_nscp_version_detail_list)     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [list](#check_nscp_version_list)                   | A list of all items which matched the filter. Common option for all checks.                                  |
| [major](#check_nscp_version_major)                 | The major (the 1 in 0.1.2.3)                                                                                 |
| [minor](#check_nscp_version_minor)                 | The minor (the 2 in 0.1.2.3)                                                                                 |
| [ok_count](#check_nscp_version_ok_count)           | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_nscp_version_ok_list)             | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [problem_count](#check_nscp_version_problem_count) | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_nscp_version_problem_list)   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [release](#check_nscp_version_release)             | The release (the 0 in 0.1.2.3)                                                                               |
| [status](#check_nscp_version_status)               | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [total](#check_nscp_version_total)                 | Total number of items. Common option for all checks.                                                         |
| [version](#check_nscp_version_version)             | The NSClient++ Version as a string                                                                           |
| [warn_count](#check_nscp_version_warn_count)       | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_nscp_version_warn_list)         | A list of all items which matched the warning criteria. Common option for all checks.                        |




