# CheckHelpers

Various helper function to extend other checks.




## Queries

A quick reference for all available queries (check commands) in the CheckHelpers module.

**List of commands:**

A list of all available queries (check commands)

| Command                                         | Description                                                                         |
|-------------------------------------------------|-------------------------------------------------------------------------------------|
| [check_always_critical](#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| [check_always_ok](#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| [check_always_warning](#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| [check_and_forward](#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| [check_critical](#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| [check_multi](#check_multi)                     | Run more then one check and return the worst state.                                 |
| [check_negate](#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| [check_ok](#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| [check_timeout](#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| [check_version](#check_version)                 | Just return the NSClient++ version.                                                 |
| [check_warning](#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| [filter_perf](#filter_perf)                     | Run a check and filter performance data.                                            |
| [render_perf](#render_perf)                     | Run a check and render the performance data as output message.                      |
| [xform_perf](#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |


**List of command aliases:**

A list of all short hand aliases for queries (check commands)


| Command             | Description                               |
|---------------------|-------------------------------------------|
| checkalwayscritical | Alias for: :query:`check_always_critical` |
| checkalwaysok       | Alias for: :query:`check_always_ok`       |
| checkalwayswarning  | Alias for: :query:`check_always_warning`  |
| checkcritical       | Alias for: :query:`check_critical`        |
| checkmultiple       | Alias for: :query:`check_multi`           |
| checkok             | Alias for: :query:`check_ok`              |
| checkversion        | Alias for: :query:`check_version`         |
| checkwarning        | Alias for: :query:`check_warning`         |
| negate              | Alias for: :query:`check_negate`          |
| timeout             | Alias for: :query:`check_timeout`         |


### check_always_critical

Run another check and regardless of its return code return CRITICAL.


* [Command-line Arguments](#check_always_critical_options)





<a name="check_always_critical_help"/>
<a name="check_always_critical_help-pb"/>
<a name="check_always_critical_show-default"/>
<a name="check_always_critical_help-short"/>
<a name="check_always_critical_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_always_ok

Run another check and regardless of its return code return OK.


* [Command-line Arguments](#check_always_ok_options)





<a name="check_always_ok_help"/>
<a name="check_always_ok_help-pb"/>
<a name="check_always_ok_show-default"/>
<a name="check_always_ok_help-short"/>
<a name="check_always_ok_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_always_warning

Run another check and regardless of its return code return WARNING.


* [Command-line Arguments](#check_always_warning_options)





<a name="check_always_warning_help"/>
<a name="check_always_warning_help-pb"/>
<a name="check_always_warning_show-default"/>
<a name="check_always_warning_help-short"/>
<a name="check_always_warning_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_and_forward

Run a check and forward the result as a passive check.


* [Command-line Arguments](#check_and_forward_options)





<a name="check_and_forward_help"/>
<a name="check_and_forward_help-pb"/>
<a name="check_and_forward_show-default"/>
<a name="check_and_forward_help-short"/>
<a name="check_and_forward_target"/>
<a name="check_and_forward_command"/>
<a name="check_and_forward_arguments"/>
<a name="check_and_forward_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |
| target       |               | Commands to run (can be used multiple times)  |
| command      |               | Commands to run (can be used multiple times)  |
| arguments    |               | List of arguments (for wrapped command)       |




### check_critical

Just return CRITICAL (anything passed along will be used as a message).


* [Command-line Arguments](#check_critical_options)





<a name="check_critical_help"/>
<a name="check_critical_help-pb"/>
<a name="check_critical_show-default"/>
<a name="check_critical_help-short"/>
<a name="check_critical_options"/>
#### Command-line Arguments


| Option                             | Default Value | Description                                   |
|------------------------------------|---------------|-----------------------------------------------|
| help                               | N/A           | Show help screen (this screen)                |
| help-pb                            | N/A           | Show help screen as a protocol buffer payload |
| show-default                       | N/A           | Show default values for a given command       |
| help-short                         | N/A           | Show help screen (short format).              |
| [message](#check_critical_message) | No message    | Message to return                             |



<h5 id="check_critical_message">message:</h5>

Message to return

*Default Value:* `No message`


### check_multi

Run more then one check and return the worst state.


* [Command-line Arguments](#check_multi_options)





<a name="check_multi_help"/>
<a name="check_multi_help-pb"/>
<a name="check_multi_show-default"/>
<a name="check_multi_help-short"/>
<a name="check_multi_command"/>
<a name="check_multi_arguments"/>
<a name="check_multi_prefix"/>
<a name="check_multi_suffix"/>
<a name="check_multi_options"/>
#### Command-line Arguments


| Option                              | Default Value | Description                                   |
|-------------------------------------|---------------|-----------------------------------------------|
| help                                | N/A           | Show help screen (this screen)                |
| help-pb                             | N/A           | Show help screen as a protocol buffer payload |
| show-default                        | N/A           | Show default values for a given command       |
| help-short                          | N/A           | Show help screen (short format).              |
| command                             |               | Commands to run (can be used multiple times)  |
| arguments                           |               | Deprecated alias for command                  |
| [separator](#check_multi_separator) | ,             | Separator between messages                    |
| prefix                              |               | Message prefix                                |
| suffix                              |               | Message suffix                                |



<h5 id="check_multi_separator">separator:</h5>

Separator between messages

*Default Value:* `, `


### check_negate

Run a check and alter the return status codes according to arguments.


* [Command-line Arguments](#check_negate_options)





<a name="check_negate_help"/>
<a name="check_negate_help-pb"/>
<a name="check_negate_show-default"/>
<a name="check_negate_help-short"/>
<a name="check_negate_ok"/>
<a name="check_negate_warning"/>
<a name="check_negate_critical"/>
<a name="check_negate_unknown"/>
<a name="check_negate_command"/>
<a name="check_negate_arguments"/>
<a name="check_negate_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |
| ok           |               | The state to return instead of OK             |
| warning      |               | The state to return instead of WARNING        |
| critical     |               | The state to return instead of CRITICAL       |
| unknown      |               | The state to return instead of UNKNOWN        |
| command      |               | Wrapped command to execute                    |
| arguments    |               | List of arguments (for wrapped command)       |




### check_ok

Just return OK (anything passed along will be used as a message).


* [Command-line Arguments](#check_ok_options)





<a name="check_ok_help"/>
<a name="check_ok_help-pb"/>
<a name="check_ok_show-default"/>
<a name="check_ok_help-short"/>
<a name="check_ok_options"/>
#### Command-line Arguments


| Option                       | Default Value | Description                                   |
|------------------------------|---------------|-----------------------------------------------|
| help                         | N/A           | Show help screen (this screen)                |
| help-pb                      | N/A           | Show help screen as a protocol buffer payload |
| show-default                 | N/A           | Show default values for a given command       |
| help-short                   | N/A           | Show help screen (short format).              |
| [message](#check_ok_message) | No message    | Message to return                             |



<h5 id="check_ok_message">message:</h5>

Message to return

*Default Value:* `No message`


### check_timeout

Run a check and timeout after a given amount of time if the check has not returned.


* [Command-line Arguments](#check_timeout_options)





<a name="check_timeout_help"/>
<a name="check_timeout_help-pb"/>
<a name="check_timeout_show-default"/>
<a name="check_timeout_help-short"/>
<a name="check_timeout_timeout"/>
<a name="check_timeout_command"/>
<a name="check_timeout_arguments"/>
<a name="check_timeout_return"/>
<a name="check_timeout_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |
| timeout      |               | The timeout value                             |
| command      |               | Wrapped command to execute                    |
| arguments    |               | List of arguments (for wrapped command)       |
| return       |               | The return status                             |




### check_version

Just return the NSClient++ version.


* [Command-line Arguments](#check_version_options)





<a name="check_version_help"/>
<a name="check_version_help-pb"/>
<a name="check_version_show-default"/>
<a name="check_version_help-short"/>
<a name="check_version_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_warning

Just return WARNING (anything passed along will be used as a message).


* [Command-line Arguments](#check_warning_options)





<a name="check_warning_help"/>
<a name="check_warning_help-pb"/>
<a name="check_warning_show-default"/>
<a name="check_warning_help-short"/>
<a name="check_warning_options"/>
#### Command-line Arguments


| Option                            | Default Value | Description                                   |
|-----------------------------------|---------------|-----------------------------------------------|
| help                              | N/A           | Show help screen (this screen)                |
| help-pb                           | N/A           | Show help screen as a protocol buffer payload |
| show-default                      | N/A           | Show default values for a given command       |
| help-short                        | N/A           | Show help screen (short format).              |
| [message](#check_warning_message) | No message    | Message to return                             |



<h5 id="check_warning_message">message:</h5>

Message to return

*Default Value:* `No message`


### filter_perf

Run a check and filter performance data.


* [Command-line Arguments](#filter_perf_options)





<a name="filter_perf_help"/>
<a name="filter_perf_help-pb"/>
<a name="filter_perf_show-default"/>
<a name="filter_perf_help-short"/>
<a name="filter_perf_command"/>
<a name="filter_perf_arguments"/>
<a name="filter_perf_options"/>
#### Command-line Arguments


| Option                      | Default Value | Description                                                 |
|-----------------------------|---------------|-------------------------------------------------------------|
| help                        | N/A           | Show help screen (this screen)                              |
| help-pb                     | N/A           | Show help screen as a protocol buffer payload               |
| show-default                | N/A           | Show default values for a given command                     |
| help-short                  | N/A           | Show help screen (short format).                            |
| [sort](#filter_perf_sort)   | none          | The sort order to use: none, normal or reversed             |
| [limit](#filter_perf_limit) | 0             | The maximum number of items to return (0 returns all items) |
| command                     |               | Wrapped command to execute                                  |
| arguments                   |               | List of arguments (for wrapped command)                     |



<h5 id="filter_perf_sort">sort:</h5>

The sort order to use: none, normal or reversed

*Default Value:* `none`

<h5 id="filter_perf_limit">limit:</h5>

The maximum number of items to return (0 returns all items)

*Default Value:* `0`


### render_perf

Run a check and render the performance data as output message.


* [Command-line Arguments](#render_perf_options)
* [Filter keywords](#render_perf_filter_keys)





<a name="render_perf_warn"/>
<a name="render_perf_crit"/>
<a name="render_perf_debug"/>
<a name="render_perf_show-all"/>
<a name="render_perf_escape-html"/>
<a name="render_perf_help"/>
<a name="render_perf_help-pb"/>
<a name="render_perf_show-default"/>
<a name="render_perf_help-short"/>
<a name="render_perf_command"/>
<a name="render_perf_arguments"/>
<a name="render_perf_remove-perf"/>
<a name="render_perf_options"/>
#### Command-line Arguments


| Option                                      | Default Value                                          | Description                                                                                                      |
|---------------------------------------------|--------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#render_perf_filter)               |                                                        | Filter which marks interesting items.                                                                            |
| [warning](#render_perf_warning)             |                                                        | Filter which marks items which generates a warning state.                                                        |
| warn                                        |                                                        | Short alias for warning                                                                                          |
| [critical](#render_perf_critical)           |                                                        | Filter which marks items which generates a critical state.                                                       |
| crit                                        |                                                        | Short alias for critical.                                                                                        |
| [ok](#render_perf_ok)                       |                                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                       | N/A                                                    | Show debugging information in the log                                                                            |
| show-all                                    | N/A                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#render_perf_empty-state)     | unknown                                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#render_perf_perf-config)     |                                                        | Performance data generation configuration                                                                        |
| escape-html                                 | N/A                                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                        | N/A                                                    | Show help screen (this screen)                                                                                   |
| help-pb                                     | N/A                                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                                | N/A                                                    | Show default values for a given command                                                                          |
| help-short                                  | N/A                                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#render_perf_top-syntax)       | %(status): %(message) %(list)                          | Top level syntax.                                                                                                |
| [ok-syntax](#render_perf_ok-syntax)         |                                                        | ok syntax.                                                                                                       |
| [empty-syntax](#render_perf_empty-syntax)   |                                                        | Empty syntax.                                                                                                    |
| [detail-syntax](#render_perf_detail-syntax) | %(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)
 | Detail level syntax.                                                                                             |
| [perf-syntax](#render_perf_perf-syntax)     | %(key)                                                 | Performance alias syntax.                                                                                        |
| command                                     |                                                        | Wrapped command to execute                                                                                       |
| arguments                                   |                                                        | List of arguments (for wrapped command)                                                                          |
| remove-perf                                 | N/A                                                    | List of arguments (for wrapped command)                                                                          |



<h5 id="render_perf_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="render_perf_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="render_perf_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="render_perf_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="render_perf_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="render_perf_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="render_perf_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `%(status): %(message) %(list)`

<h5 id="render_perf_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="render_perf_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="render_perf_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `%(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)
`

<h5 id="render_perf_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `%(key)`


<a name="render_perf_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit          | Major version number                                                                                         |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| key           | Major version number                                                                                         |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| max           | Major version number                                                                                         |
| message       | Major version number                                                                                         |
| min           | Major version number                                                                                         |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| unit          | Major version number                                                                                         |
| value         | Major version number                                                                                         |
| warn          | Major version number                                                                                         |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### xform_perf

Run a check and transform the performance data in various (currently one) way.


* [Command-line Arguments](#xform_perf_options)





<a name="xform_perf_help"/>
<a name="xform_perf_help-pb"/>
<a name="xform_perf_show-default"/>
<a name="xform_perf_help-short"/>
<a name="xform_perf_command"/>
<a name="xform_perf_arguments"/>
<a name="xform_perf_mode"/>
<a name="xform_perf_field"/>
<a name="xform_perf_replace"/>
<a name="xform_perf_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                                                 |
|--------------|---------------|-----------------------------------------------------------------------------|
| help         | N/A           | Show help screen (this screen)                                              |
| help-pb      | N/A           | Show help screen as a protocol buffer payload                               |
| show-default | N/A           | Show default values for a given command                                     |
| help-short   | N/A           | Show help screen (short format).                                            |
| command      |               | Wrapped command to execute                                                  |
| arguments    |               | List of arguments (for wrapped command)                                     |
| mode         |               | Transformation mode: extract to fetch data or minmax to add missing min/max |
| field        |               | Field to work with (value, warn, crit, max, min)                            |
| replace      |               | Replace expression for the alias                                            |






