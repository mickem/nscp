# CheckTaskSched

Check status of your scheduled jobs.



## List of commands

A list of all available queries (check commands)

| Command                             | Description                       |
|-------------------------------------|-----------------------------------|
| [check_tasksched](#check_tasksched) | Check status of scheduled jobs.   |
| [checktasksched](#checktasksched)   | Legacy version of check_tasksched |







# Queries

A quick reference for all available queries (check commands) in the CheckTaskSched module.

## check_tasksched

Check status of scheduled jobs.

### Usage

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckTaskSched_check_tasksched_samples.md)_

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_tasksched
/test: 1 != 0|'test'=1;0;0
```

### Usage


| Option                                          | Default Value                         | Description                                                                                                                            |
|-------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_tasksched_filter)               | enabled = 1                           | Filter which marks interesting items.                                                                                                  |
| [warning](#check_tasksched_warning)             | exit_code != 0                        | Filter which marks items which generates a warning state.                                                                              |
| [warn](#check_tasksched_warn)                   |                                       | Short alias for warning                                                                                                                |
| [critical](#check_tasksched_critical)           | exit_code < 0                         | Filter which marks items which generates a critical state.                                                                             |
| [crit](#check_tasksched_crit)                   |                                       | Short alias for critical.                                                                                                              |
| [ok](#check_tasksched_ok)                       |                                       | Filter which marks items which generates an ok state.                                                                                  |
| [debug](#check_tasksched_debug)                 | N/A                                   | Show debugging information in the log                                                                                                  |
| [show-all](#check_tasksched_show-all)           | N/A                                   | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                       |
| [empty-state](#check_tasksched_empty-state)     | warning                               | Return status to use when nothing matched filter.                                                                                      |
| [perf-config](#check_tasksched_perf-config)     |                                       | Performance data generation configuration                                                                                              |
| [escape-html](#check_tasksched_escape-html)     | N/A                                   | Escape any < and > characters to prevent HTML encoding                                                                                 |
| [help](#check_tasksched_help)                   | N/A                                   | Show help screen (this screen)                                                                                                         |
| [help-pb](#check_tasksched_help-pb)             | N/A                                   | Show help screen as a protocol buffer payload                                                                                          |
| [show-default](#check_tasksched_show-default)   | N/A                                   | Show default values for a given command                                                                                                |
| [help-short](#check_tasksched_help-short)       | N/A                                   | Show help screen (short format).                                                                                                       |
| [top-syntax](#check_tasksched_top-syntax)       | ${status}: ${problem_list}            | Top level syntax.                                                                                                                      |
| [ok-syntax](#check_tasksched_ok-syntax)         | %(status): All tasks are ok           | ok syntax.                                                                                                                             |
| [empty-syntax](#check_tasksched_empty-syntax)   | %(status): No tasks found             | Empty syntax.                                                                                                                          |
| [detail-syntax](#check_tasksched_detail-syntax) | ${folder}/${title}: ${exit_code} != 0 | Detail level syntax.                                                                                                                   |
| [perf-syntax](#check_tasksched_perf-syntax)     | ${title}                              | Performance alias syntax.                                                                                                              |
| [force-old](#check_tasksched_force-old)         | N/A                                   | The name of the computer that you want to connect to.                                                                                  |
| [computer](#check_tasksched_computer)           |                                       | The name of the computer that you want to connect to.                                                                                  |
| [user](#check_tasksched_user)                   |                                       | The user name that is used during the connection to the computer.                                                                      |
| [domain](#check_tasksched_domain)               |                                       | The domain of the user specified in the user parameter.                                                                                |
| [password](#check_tasksched_password)           |                                       | The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used. |
| [folder](#check_tasksched_folder)               |                                       | The folder in which the tasks to check reside.                                                                                         |
| [recursive](#check_tasksched_recursive)         |                                       | Recurse sub folder (defaults to true).                                                                                                 |


<a name="check_tasksched_filter"/>
### filter


**Deafult Value:** enabled = 1

**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key                  | Value                                                                                                         |
|----------------------|---------------------------------------------------------------------------------------------------------------|
| count                | Number of items matching the filter. Common option for all checks.                                            |
| total                |  Total number of items. Common option for all checks.                                                         |
| ok_count             |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count           |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count           |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count        |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list                 |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list              |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list            |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list            |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list         |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list          |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status               |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| application          | Retrieves the name of the application that the task is associated with.                                       |
| comment              | Retrieves the comment or description for the work item.                                                       |
| creator              | Retrieves the creator of the work item.                                                                       |
| enabled              | TODO.                                                                                                         |
| exit_code            | Retrieves the work item's last exit code.                                                                     |
| folder               | The task folder                                                                                               |
| has_run              | True if the task has ever executed.                                                                           |
| max_run_time         | Retrieves the maximum length of time the task can run.                                                        |
| most_recent_run_time | Retrieves the most recent time the work item began running.                                                   |
| parameters           | Retrieves the command-line parameters of a task.                                                              |
| priority             | Retrieves the priority for the task.                                                                          |
| task_status          | Retrieves the status of the work item.                                                                        |
| title                | The task title                                                                                                |
| working_directory    | Retrieves the working directory of the task.                                                                  |







<a name="check_tasksched_warning"/>
### warning


**Deafult Value:** exit_code != 0

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key                  | Value                                                                                                         |
|----------------------|---------------------------------------------------------------------------------------------------------------|
| count                | Number of items matching the filter. Common option for all checks.                                            |
| total                |  Total number of items. Common option for all checks.                                                         |
| ok_count             |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count           |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count           |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count        |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list                 |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list              |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list            |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list            |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list         |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list          |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status               |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| application          | Retrieves the name of the application that the task is associated with.                                       |
| comment              | Retrieves the comment or description for the work item.                                                       |
| creator              | Retrieves the creator of the work item.                                                                       |
| enabled              | TODO.                                                                                                         |
| exit_code            | Retrieves the work item's last exit code.                                                                     |
| folder               | The task folder                                                                                               |
| has_run              | True if the task has ever executed.                                                                           |
| max_run_time         | Retrieves the maximum length of time the task can run.                                                        |
| most_recent_run_time | Retrieves the most recent time the work item began running.                                                   |
| parameters           | Retrieves the command-line parameters of a task.                                                              |
| priority             | Retrieves the priority for the task.                                                                          |
| task_status          | Retrieves the status of the work item.                                                                        |
| title                | The task title                                                                                                |
| working_directory    | Retrieves the working directory of the task.                                                                  |







<a name="check_tasksched_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_tasksched_critical"/>
### critical


**Deafult Value:** exit_code < 0

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key                  | Value                                                                                                         |
|----------------------|---------------------------------------------------------------------------------------------------------------|
| count                | Number of items matching the filter. Common option for all checks.                                            |
| total                |  Total number of items. Common option for all checks.                                                         |
| ok_count             |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count           |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count           |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count        |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list                 |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list              |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list            |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list            |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list         |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list          |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status               |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| application          | Retrieves the name of the application that the task is associated with.                                       |
| comment              | Retrieves the comment or description for the work item.                                                       |
| creator              | Retrieves the creator of the work item.                                                                       |
| enabled              | TODO.                                                                                                         |
| exit_code            | Retrieves the work item's last exit code.                                                                     |
| folder               | The task folder                                                                                               |
| has_run              | True if the task has ever executed.                                                                           |
| max_run_time         | Retrieves the maximum length of time the task can run.                                                        |
| most_recent_run_time | Retrieves the most recent time the work item began running.                                                   |
| parameters           | Retrieves the command-line parameters of a task.                                                              |
| priority             | Retrieves the priority for the task.                                                                          |
| task_status          | Retrieves the status of the work item.                                                                        |
| title                | The task title                                                                                                |
| working_directory    | Retrieves the working directory of the task.                                                                  |







<a name="check_tasksched_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_tasksched_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key                  | Value                                                                                                         |
|----------------------|---------------------------------------------------------------------------------------------------------------|
| count                | Number of items matching the filter. Common option for all checks.                                            |
| total                |  Total number of items. Common option for all checks.                                                         |
| ok_count             |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count           |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count           |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count        |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list                 |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list              |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list            |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list            |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list         |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list          |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status               |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| application          | Retrieves the name of the application that the task is associated with.                                       |
| comment              | Retrieves the comment or description for the work item.                                                       |
| creator              | Retrieves the creator of the work item.                                                                       |
| enabled              | TODO.                                                                                                         |
| exit_code            | Retrieves the work item's last exit code.                                                                     |
| folder               | The task folder                                                                                               |
| has_run              | True if the task has ever executed.                                                                           |
| max_run_time         | Retrieves the maximum length of time the task can run.                                                        |
| most_recent_run_time | Retrieves the most recent time the work item began running.                                                   |
| parameters           | Retrieves the command-line parameters of a task.                                                              |
| priority             | Retrieves the priority for the task.                                                                          |
| task_status          | Retrieves the status of the work item.                                                                        |
| title                | The task title                                                                                                |
| working_directory    | Retrieves the working directory of the task.                                                                  |







<a name="check_tasksched_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_tasksched_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_tasksched_empty-state"/>
### empty-state


**Deafult Value:** warning

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_tasksched_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_tasksched_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_tasksched_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_tasksched_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_tasksched_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_tasksched_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_tasksched_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${problem_list}

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






<a name="check_tasksched_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): All tasks are ok

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_tasksched_empty-syntax"/>
### empty-syntax


**Deafult Value:** %(status): No tasks found

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






<a name="check_tasksched_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${folder}/${title}: ${exit_code} != 0

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key                  | Value                                                                   |
|----------------------|-------------------------------------------------------------------------|
| application          | Retrieves the name of the application that the task is associated with. |
| comment              | Retrieves the comment or description for the work item.                 |
| creator              | Retrieves the creator of the work item.                                 |
| enabled              | TODO.                                                                   |
| exit_code            | Retrieves the work item's last exit code.                               |
| folder               | The task folder                                                         |
| has_run              | True if the task has ever executed.                                     |
| max_run_time         | Retrieves the maximum length of time the task can run.                  |
| most_recent_run_time | Retrieves the most recent time the work item began running.             |
| parameters           | Retrieves the command-line parameters of a task.                        |
| priority             | Retrieves the priority for the task.                                    |
| task_status          | Retrieves the status of the work item.                                  |
| title                | The task title                                                          |
| working_directory    | Retrieves the working directory of the task.                            |






<a name="check_tasksched_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${title}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key                  | Value                                                                   |
|----------------------|-------------------------------------------------------------------------|
| application          | Retrieves the name of the application that the task is associated with. |
| comment              | Retrieves the comment or description for the work item.                 |
| creator              | Retrieves the creator of the work item.                                 |
| enabled              | TODO.                                                                   |
| exit_code            | Retrieves the work item's last exit code.                               |
| folder               | The task folder                                                         |
| has_run              | True if the task has ever executed.                                     |
| max_run_time         | Retrieves the maximum length of time the task can run.                  |
| most_recent_run_time | Retrieves the most recent time the work item began running.             |
| parameters           | Retrieves the command-line parameters of a task.                        |
| priority             | Retrieves the priority for the task.                                    |
| task_status          | Retrieves the status of the work item.                                  |
| title                | The task title                                                          |
| working_directory    | Retrieves the working directory of the task.                            |






<a name="check_tasksched_force-old"/>
### force-old



**Description:**
The name of the computer that you want to connect to.

<a name="check_tasksched_computer"/>
### computer



**Description:**
The name of the computer that you want to connect to.

<a name="check_tasksched_user"/>
### user



**Description:**
The user name that is used during the connection to the computer.

<a name="check_tasksched_domain"/>
### domain



**Description:**
The domain of the user specified in the user parameter.

<a name="check_tasksched_password"/>
### password



**Description:**
The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used.

<a name="check_tasksched_folder"/>
### folder



**Description:**
The folder in which the tasks to check reside.

<a name="check_tasksched_recursive"/>
### recursive



**Description:**
Recurse sub folder (defaults to true).

## checktasksched

Legacy version of check_tasksched


### Usage


| Option                                         | Default Value | Description                                                     |
|------------------------------------------------|---------------|-----------------------------------------------------------------|
| [help](#checktasksched_help)                   | N/A           | Show help screen (this screen)                                  |
| [help-pb](#checktasksched_help-pb)             | N/A           | Show help screen as a protocol buffer payload                   |
| [show-default](#checktasksched_show-default)   | N/A           | Show default values for a given command                         |
| [help-short](#checktasksched_help-short)       | N/A           | Show help screen (short format).                                |
| [warn](#checktasksched_warn)                   |               | Warning bounds.                                                 |
| [crit](#checktasksched_crit)                   |               | Critical bounds.                                                |
| [MaxWarn](#checktasksched_MaxWarn)             |               | Maximum value before a warning is returned.                     |
| [MaxCrit](#checktasksched_MaxCrit)             |               | Maximum value before a critical is returned.                    |
| [MinWarn](#checktasksched_MinWarn)             |               | Minimum value before a warning is returned.                     |
| [MinCrit](#checktasksched_MinCrit)             |               | Minimum value before a critical is returned.                    |
| [Counter](#checktasksched_Counter)             |               | The time to check                                               |
| [truncate](#checktasksched_truncate)           |               | Deprecated option                                               |
| [syntax](#checktasksched_syntax)               |               | Syntax (same as detail-syntax in the check_tasksched check)     |
| [master-syntax](#checktasksched_master-syntax) |               | Master Syntax (same as top-syntax in the check_tasksched check) |
| [filter](#checktasksched_filter)               |               | Filter (same as filter in the check_tasksched check)            |
| [debug](#checktasksched_debug)                 | N/A           | Filter (same as filter in the check_tasksched check)            |


<a name="checktasksched_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checktasksched_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checktasksched_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checktasksched_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checktasksched_warn"/>
### warn



**Description:**
Warning bounds.

<a name="checktasksched_crit"/>
### crit



**Description:**
Critical bounds.

<a name="checktasksched_MaxWarn"/>
### MaxWarn



**Description:**
Maximum value before a warning is returned.

<a name="checktasksched_MaxCrit"/>
### MaxCrit



**Description:**
Maximum value before a critical is returned.

<a name="checktasksched_MinWarn"/>
### MinWarn



**Description:**
Minimum value before a warning is returned.

<a name="checktasksched_MinCrit"/>
### MinCrit



**Description:**
Minimum value before a critical is returned.

<a name="checktasksched_Counter"/>
### Counter



**Description:**
The time to check

<a name="checktasksched_truncate"/>
### truncate



**Description:**
Deprecated option

<a name="checktasksched_syntax"/>
### syntax



**Description:**
Syntax (same as detail-syntax in the check_tasksched check)

<a name="checktasksched_master-syntax"/>
### master-syntax



**Description:**
Master Syntax (same as top-syntax in the check_tasksched check)

<a name="checktasksched_filter"/>
### filter



**Description:**
Filter (same as filter in the check_tasksched check)

<a name="checktasksched_debug"/>
### debug



**Description:**
Filter (same as filter in the check_tasksched check)



