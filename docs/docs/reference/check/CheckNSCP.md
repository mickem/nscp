# CheckNSCP

Use this module to check the healt and status of NSClient++ it self



## List of commands

A list of all available queries (check commands)

| Command                                   | Description                                    |
|-------------------------------------------|------------------------------------------------|
| [check_nscp](#check_nscp)                 | Check the internal healt of NSClient++.        |
| [check_nscp_version](#check_nscp_version) | Check the version of NSClient++ which is used. |







# Queries

A quick reference for all available queries (check commands) in the CheckNSCP module.

## check_nscp

Check the internal healt of NSClient++.


### Usage


| Option                                   | Default Value | Description                                   |
|------------------------------------------|---------------|-----------------------------------------------|
| [help](#check_nscp_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#check_nscp_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#check_nscp_show-default) | N/A           | Show default values for a given command       |
| [help-short](#check_nscp_help-short)     | N/A           | Show help screen (short format).              |


<a name="check_nscp_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_nscp_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_nscp_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_nscp_help-short"/>
### help-short



**Description:**
Show help screen (short format).

## check_nscp_version

Check the version of NSClient++ which is used.


### Usage


| Option                                             | Default Value                                   | Description                                                                                                      |
|----------------------------------------------------|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_nscp_version_filter)               |                                                 | Filter which marks interesting items.                                                                            |
| [warning](#check_nscp_version_warning)             |                                                 | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_nscp_version_warn)                   |                                                 | Short alias for warning                                                                                          |
| [critical](#check_nscp_version_critical)           |                                                 | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_nscp_version_crit)                   |                                                 | Short alias for critical.                                                                                        |
| [ok](#check_nscp_version_ok)                       |                                                 | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_nscp_version_debug)                 | N/A                                             | Show debugging information in the log                                                                            |
| [show-all](#check_nscp_version_show-all)           | N/A                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_nscp_version_empty-state)     | ignored                                         | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_nscp_version_perf-config)     |                                                 | Performance data generation configuration                                                                        |
| [escape-html](#check_nscp_version_escape-html)     | N/A                                             | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_nscp_version_help)                   | N/A                                             | Show help screen (this screen)                                                                                   |
| [help-pb](#check_nscp_version_help-pb)             | N/A                                             | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_nscp_version_show-default)   | N/A                                             | Show default values for a given command                                                                          |
| [help-short](#check_nscp_version_help-short)       | N/A                                             | Show help screen (short format).                                                                                 |
| [top-syntax](#check_nscp_version_top-syntax)       | ${status}: ${list}                              | Top level syntax.                                                                                                |
| [ok-syntax](#check_nscp_version_ok-syntax)         |                                                 | ok syntax.                                                                                                       |
| [empty-syntax](#check_nscp_version_empty-syntax)   |                                                 | Empty syntax.                                                                                                    |
| [detail-syntax](#check_nscp_version_detail-syntax) | ${release}.${major}.${minor}.${build} (${date}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_nscp_version_perf-syntax)     | version                                         | Performance alias syntax.                                                                                        |


<a name="check_nscp_version_filter"/>
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
| build         | The build (the 3 in 0.1.2.3)                                                                                  |
| date          | The NSClient++ Build date                                                                                     |
| major         | The major (the 1 in 0.1.2.3)                                                                                  |
| minor         | The minor (the 2 in 0.1.2.3)                                                                                  |
| release       | The release (the 0 in 0.1.2.3)                                                                                |
| version       | The NSClient++ Version as a string                                                                            |







<a name="check_nscp_version_warning"/>
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
| build         | The build (the 3 in 0.1.2.3)                                                                                  |
| date          | The NSClient++ Build date                                                                                     |
| major         | The major (the 1 in 0.1.2.3)                                                                                  |
| minor         | The minor (the 2 in 0.1.2.3)                                                                                  |
| release       | The release (the 0 in 0.1.2.3)                                                                                |
| version       | The NSClient++ Version as a string                                                                            |







<a name="check_nscp_version_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_nscp_version_critical"/>
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
| build         | The build (the 3 in 0.1.2.3)                                                                                  |
| date          | The NSClient++ Build date                                                                                     |
| major         | The major (the 1 in 0.1.2.3)                                                                                  |
| minor         | The minor (the 2 in 0.1.2.3)                                                                                  |
| release       | The release (the 0 in 0.1.2.3)                                                                                |
| version       | The NSClient++ Version as a string                                                                            |







<a name="check_nscp_version_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_nscp_version_ok"/>
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
| build         | The build (the 3 in 0.1.2.3)                                                                                  |
| date          | The NSClient++ Build date                                                                                     |
| major         | The major (the 1 in 0.1.2.3)                                                                                  |
| minor         | The minor (the 2 in 0.1.2.3)                                                                                  |
| release       | The release (the 0 in 0.1.2.3)                                                                                |
| version       | The NSClient++ Version as a string                                                                            |







<a name="check_nscp_version_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_nscp_version_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_nscp_version_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_nscp_version_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_nscp_version_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_nscp_version_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_nscp_version_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_nscp_version_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_nscp_version_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_nscp_version_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

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






<a name="check_nscp_version_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_nscp_version_empty-syntax"/>
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






<a name="check_nscp_version_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${release}.${major}.${minor}.${build} (${date})

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                              |
|---------|------------------------------------|
| build   | The build (the 3 in 0.1.2.3)       |
| date    | The NSClient++ Build date          |
| major   | The major (the 1 in 0.1.2.3)       |
| minor   | The minor (the 2 in 0.1.2.3)       |
| release | The release (the 0 in 0.1.2.3)     |
| version | The NSClient++ Version as a string |






<a name="check_nscp_version_perf-syntax"/>
### perf-syntax


**Deafult Value:** version

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                              |
|---------|------------------------------------|
| build   | The build (the 3 in 0.1.2.3)       |
| date    | The NSClient++ Build date          |
| major   | The major (the 1 in 0.1.2.3)       |
| minor   | The minor (the 2 in 0.1.2.3)       |
| release | The release (the 0 in 0.1.2.3)     |
| version | The NSClient++ Version as a string |








