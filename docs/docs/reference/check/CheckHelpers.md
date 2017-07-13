# CheckHelpers

Various helper function to extend other checks.



## List of commands

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


## List of command aliases

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





# Queries

A quick reference for all available queries (check commands) in the CheckHelpers module.

## check_always_critical

Run another check and regardless of its return code return CRITICAL.


### Usage


| Option                                              | Default Value | Description                                   |
|-----------------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_always_critical_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_always_critical_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_always_critical_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_always_critical_help-short)     | N/A           | Show help screen (short format).              |


<a name="check_always_critical_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_always_critical_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_always_critical_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_always_critical_help-short"/>
### help-short



**Description:**
Show help screen (short format).

## check_always_ok

Run another check and regardless of its return code return OK.


### Usage


| Option                                        | Default Value | Description                                   |
|-----------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_always_ok_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_always_ok_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_always_ok_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_always_ok_help-short)     | N/A           | Show help screen (short format).              |


<a name="check_always_ok_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_always_ok_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_always_ok_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_always_ok_help-short"/>
### help-short



**Description:**
Show help screen (short format).

## check_always_warning

Run another check and regardless of its return code return WARNING.


### Usage


| Option                                             | Default Value | Description                                   |
|----------------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_always_warning_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_always_warning_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_always_warning_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_always_warning_help-short)     | N/A           | Show help screen (short format).              |


<a name="check_always_warning_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_always_warning_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_always_warning_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_always_warning_help-short"/>
### help-short



**Description:**
Show help screen (short format).

## check_and_forward

Run a check and forward the result as a passive check.


### Usage


| Option                                          | Default Value | Description                                   |
|-------------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_and_forward_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_and_forward_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_and_forward_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_and_forward_help-short)     | N/A           | Show help screen (short format).              |
| [target](#check_and_forward_target)             |               | Commands to run (can be used multiple times)  |
| [command](#check_and_forward_command)           |               | Commands to run (can be used multiple times)  |
| [arguments](#check_and_forward_arguments)       |               | List of arguments (for wrapped command)       |


<a name="check_and_forward_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_and_forward_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_and_forward_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_and_forward_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_and_forward_target"/>
### target



**Description:**
Commands to run (can be used multiple times)

<a name="check_and_forward_command"/>
### command



**Description:**
Commands to run (can be used multiple times)

<a name="check_and_forward_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

## check_critical

Just return CRITICAL (anything passed along will be used as a message).


### Usage


| Option                                       | Default Value | Description                                   |
|----------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_critical_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_critical_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_critical_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_critical_help-short)     | N/A           | Show help screen (short format).              |
| [message](#check_critical_message)           | No message    | Message to return                             |


<a name="check_critical_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_critical_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_critical_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_critical_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_critical_message"/>
### message


**Deafult Value:** No message

**Description:**
Message to return

## check_multi

Run more then one check and return the worst state.


### Usage


| Option                                    | Default Value | Description                                   |
|-------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_multi_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_multi_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_multi_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_multi_help-short)     | N/A           | Show help screen (short format).              |
| [command](#check_multi_command)           |               | Commands to run (can be used multiple times)  |
| [arguments](#check_multi_arguments)       |               | Deprecated alias for command                  |
| [separator](#check_multi_separator)       | ,             | Separator between messages                    |
| [prefix](#check_multi_prefix)             |               | Message prefix                                |
| [suffix](#check_multi_suffix)             |               | Message suffix                                |


<a name="check_multi_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_multi_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_multi_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_multi_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_multi_command"/>
### command



**Description:**
Commands to run (can be used multiple times)

<a name="check_multi_arguments"/>
### arguments



**Description:**
Deprecated alias for command

<a name="check_multi_separator"/>
### separator


**Deafult Value:** , 

**Description:**
Separator between messages

<a name="check_multi_prefix"/>
### prefix



**Description:**
Message prefix

<a name="check_multi_suffix"/>
### suffix



**Description:**
Message suffix

## check_negate

Run a check and alter the return status codes according to arguments.


### Usage


| Option                                     | Default Value | Description                                   |
|--------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_negate_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_negate_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_negate_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_negate_help-short)     | N/A           | Show help screen (short format).              |
| [ok](#check_negate_ok)                     |               | The state to return instead of OK             |
| [warning](#check_negate_warning)           |               | The state to return instead of WARNING        |
| [critical](#check_negate_critical)         |               | The state to return instead of CRITICAL       |
| [unknown](#check_negate_unknown)           |               | The state to return instead of UNKNOWN        |
| [command](#check_negate_command)           |               | Wrapped command to execute                    |
| [arguments](#check_negate_arguments)       |               | List of arguments (for wrapped command)       |


<a name="check_negate_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_negate_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_negate_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_negate_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_negate_ok"/>
### ok



**Description:**
The state to return instead of OK

<a name="check_negate_warning"/>
### warning



**Description:**
The state to return instead of WARNING

<a name="check_negate_critical"/>
### critical



**Description:**
The state to return instead of CRITICAL

<a name="check_negate_unknown"/>
### unknown



**Description:**
The state to return instead of UNKNOWN

<a name="check_negate_command"/>
### command



**Description:**
Wrapped command to execute

<a name="check_negate_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

## check_ok

Just return OK (anything passed along will be used as a message).


### Usage


| Option                                 | Default Value | Description                                   |
|----------------------------------------|---------------|-----------------------------------------------|
| [help](#check_ok_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_ok_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_ok_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_ok_help-short)     | N/A           | Show help screen (short format).              |
| [message](#check_ok_message)           | No message    | Message to return                             |


<a name="check_ok_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_ok_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_ok_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_ok_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_ok_message"/>
### message


**Deafult Value:** No message

**Description:**
Message to return

## check_timeout

Run a check and timeout after a given amount of time if the check has not returned.


### Usage


| Option                                      | Default Value | Description                                   |
|---------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_timeout_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_timeout_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_timeout_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_timeout_help-short)     | N/A           | Show help screen (short format).              |
| [timeout](#check_timeout_timeout)           |               | The timeout value                             |
| [command](#check_timeout_command)           |               | Wrapped command to execute                    |
| [arguments](#check_timeout_arguments)       |               | List of arguments (for wrapped command)       |
| [return](#check_timeout_return)             |               | The return status                             |


<a name="check_timeout_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_timeout_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_timeout_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_timeout_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_timeout_timeout"/>
### timeout



**Description:**
The timeout value

<a name="check_timeout_command"/>
### command



**Description:**
Wrapped command to execute

<a name="check_timeout_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

<a name="check_timeout_return"/>
### return



**Description:**
The return status

## check_version

Just return the NSClient++ version.


### Usage


| Option                                      | Default Value | Description                                   |
|---------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_version_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_version_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_version_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_version_help-short)     | N/A           | Show help screen (short format).              |


<a name="check_version_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_version_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_version_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_version_help-short"/>
### help-short



**Description:**
Show help screen (short format).

## check_warning

Just return WARNING (anything passed along will be used as a message).


### Usage


| Option                                      | Default Value | Description                                   |
|---------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_warning_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_warning_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_warning_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_warning_help-short)     | N/A           | Show help screen (short format).              |
| [message](#check_warning_message)           | No message    | Message to return                             |


<a name="check_warning_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_warning_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_warning_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_warning_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_warning_message"/>
### message


**Deafult Value:** No message

**Description:**
Message to return

## filter_perf

Run a check and filter performance data.


### Usage


| Option                                    | Default Value | Description                                                 |
|-------------------------------------------|---------------|-------------------------------------------------------------|
| [help](#filter_perf_help)                 | N/A           | Show help screen (this screen)                              |
| [help-pb](#filter_perf_help-pb)           | N/A           | Show help screen as a protocol buffer payload               |
| [show-default](#filter_perf_show-default) | N/A           | Show default values for a given command                     |
| [help-short](#filter_perf_help-short)     | N/A           | Show help screen (short format).                            |
| [sort](#filter_perf_sort)                 | none          | The sort order to use: none, normal or reversed             |
| [limit](#filter_perf_limit)               | 0             | The maximum number of items to return (0 returns all items) |
| [command](#filter_perf_command)           |               | Wrapped command to execute                                  |
| [arguments](#filter_perf_arguments)       |               | List of arguments (for wrapped command)                     |


<a name="filter_perf_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="filter_perf_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="filter_perf_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="filter_perf_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="filter_perf_sort"/>
### sort


**Deafult Value:** none

**Description:**
The sort order to use: none, normal or reversed

<a name="filter_perf_limit"/>
### limit


**Deafult Value:** 0

**Description:**
The maximum number of items to return (0 returns all items)

<a name="filter_perf_command"/>
### command



**Description:**
Wrapped command to execute

<a name="filter_perf_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

## render_perf

Run a check and render the performance data as output message.


### Usage


| Option                                      | Default Value                                          | Description                                                                                                      |
|---------------------------------------------|--------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#render_perf_filter)               |                                                        | Filter which marks interesting items.                                                                            |
| [warning](#render_perf_warning)             |                                                        | Filter which marks items which generates a warning state.                                                        |
| [warn](#render_perf_warn)                   |                                                        | Short alias for warning                                                                                          |
| [critical](#render_perf_critical)           |                                                        | Filter which marks items which generates a critical state.                                                       |
| [crit](#render_perf_crit)                   |                                                        | Short alias for critical.                                                                                        |
| [ok](#render_perf_ok)                       |                                                        | Filter which marks items which generates an ok state.                                                            |
| [debug](#render_perf_debug)                 | N/A                                                    | Show debugging information in the log                                                                            |
| [show-all](#render_perf_show-all)           | N/A                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#render_perf_empty-state)     | unknown                                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#render_perf_perf-config)     |                                                        | Performance data generation configuration                                                                        |
| [escape-html](#render_perf_escape-html)     | N/A                                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#render_perf_help)                   | N/A                                                    | Show help screen (this screen)                                                                                   |
| [help-pb](#render_perf_help-pb)             | N/A                                                    | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#render_perf_show-default)   | N/A                                                    | Show default values for a given command                                                                          |
| [help-short](#render_perf_help-short)       | N/A                                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#render_perf_top-syntax)       | %(status): %(message) %(list)                          | Top level syntax.                                                                                                |
| [ok-syntax](#render_perf_ok-syntax)         |                                                        | ok syntax.                                                                                                       |
| [empty-syntax](#render_perf_empty-syntax)   |                                                        | Empty syntax.                                                                                                    |
| [detail-syntax](#render_perf_detail-syntax) | %(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)
 | Detail level syntax.                                                                                             |
| [perf-syntax](#render_perf_perf-syntax)     | %(key)                                                 | Performance alias syntax.                                                                                        |
| [command](#render_perf_command)             |                                                        | Wrapped command to execute                                                                                       |
| [arguments](#render_perf_arguments)         |                                                        | List of arguments (for wrapped command)                                                                          |
| [remove-perf](#render_perf_remove-perf)     | N/A                                                    | List of arguments (for wrapped command)                                                                          |


<a name="render_perf_filter"/>
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
| crit          | Major version number                                                                                          |
| key           | Major version number                                                                                          |
| max           | Major version number                                                                                          |
| message       | Major version number                                                                                          |
| min           | Major version number                                                                                          |
| unit          | Major version number                                                                                          |
| value         | Major version number                                                                                          |
| warn          | Major version number                                                                                          |







<a name="render_perf_warning"/>
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
| crit          | Major version number                                                                                          |
| key           | Major version number                                                                                          |
| max           | Major version number                                                                                          |
| message       | Major version number                                                                                          |
| min           | Major version number                                                                                          |
| unit          | Major version number                                                                                          |
| value         | Major version number                                                                                          |
| warn          | Major version number                                                                                          |







<a name="render_perf_warn"/>
### warn



**Description:**
Short alias for warning

<a name="render_perf_critical"/>
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
| crit          | Major version number                                                                                          |
| key           | Major version number                                                                                          |
| max           | Major version number                                                                                          |
| message       | Major version number                                                                                          |
| min           | Major version number                                                                                          |
| unit          | Major version number                                                                                          |
| value         | Major version number                                                                                          |
| warn          | Major version number                                                                                          |







<a name="render_perf_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="render_perf_ok"/>
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
| crit          | Major version number                                                                                          |
| key           | Major version number                                                                                          |
| max           | Major version number                                                                                          |
| message       | Major version number                                                                                          |
| min           | Major version number                                                                                          |
| unit          | Major version number                                                                                          |
| value         | Major version number                                                                                          |
| warn          | Major version number                                                                                          |







<a name="render_perf_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="render_perf_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="render_perf_empty-state"/>
### empty-state


**Deafult Value:** unknown

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="render_perf_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="render_perf_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="render_perf_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="render_perf_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="render_perf_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="render_perf_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="render_perf_top-syntax"/>
### top-syntax


**Deafult Value:** %(status): %(message) %(list)

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






<a name="render_perf_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="render_perf_empty-syntax"/>
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






<a name="render_perf_detail-syntax"/>
### detail-syntax


**Deafult Value:** %(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)


**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                |
|---------|----------------------|
| crit    | Major version number |
| key     | Major version number |
| max     | Major version number |
| message | Major version number |
| min     | Major version number |
| unit    | Major version number |
| value   | Major version number |
| warn    | Major version number |






<a name="render_perf_perf-syntax"/>
### perf-syntax


**Deafult Value:** %(key)

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                |
|---------|----------------------|
| crit    | Major version number |
| key     | Major version number |
| max     | Major version number |
| message | Major version number |
| min     | Major version number |
| unit    | Major version number |
| value   | Major version number |
| warn    | Major version number |






<a name="render_perf_command"/>
### command



**Description:**
Wrapped command to execute

<a name="render_perf_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

<a name="render_perf_remove-perf"/>
### remove-perf



**Description:**
List of arguments (for wrapped command)

## xform_perf

Run a check and transform the performance data in various (currently one) way.


### Usage


| Option                                   | Default Value | Description                                                                 |
|------------------------------------------|---------------|-----------------------------------------------------------------------------|
| [help](#xform_perf_help)                 | N/A           | Show help screen (this screen)                                              |
| [help-pb](#xform_perf_help-pb)           | N/A           | Show help screen as a protocol buffer payload                               |
| [show-default](#xform_perf_show-default) | N/A           | Show default values for a given command                                     |
| [help-short](#xform_perf_help-short)     | N/A           | Show help screen (short format).                                            |
| [command](#xform_perf_command)           |               | Wrapped command to execute                                                  |
| [arguments](#xform_perf_arguments)       |               | List of arguments (for wrapped command)                                     |
| [mode](#xform_perf_mode)                 |               | Transformation mode: extract to fetch data or minmax to add missing min/max |
| [field](#xform_perf_field)               |               | Field to work with (value, warn, crit, max, min)                            |
| [replace](#xform_perf_replace)           |               | Replace expression for the alias                                            |


<a name="xform_perf_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="xform_perf_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="xform_perf_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="xform_perf_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="xform_perf_command"/>
### command



**Description:**
Wrapped command to execute

<a name="xform_perf_arguments"/>
### arguments



**Description:**
List of arguments (for wrapped command)

<a name="xform_perf_mode"/>
### mode



**Description:**
Transformation mode: extract to fetch data or minmax to add missing min/max

<a name="xform_perf_field"/>
### field



**Description:**
Field to work with (value, warn, crit, max, min)

<a name="xform_perf_replace"/>
### replace



**Description:**
Replace expression for the alias



