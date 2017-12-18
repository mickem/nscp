# CheckTaskSched

Check status of your scheduled jobs.



**List of commands:**

A list of all available queries (check commands)

| Command                             | Description                       |
|-------------------------------------|-----------------------------------|
| [check_tasksched](#check_tasksched) | Check status of scheduled jobs.   |
| [checktasksched](#checktasksched)   | Legacy version of check_tasksched |







## Queries

A quick reference for all available queries (check commands) in the CheckTaskSched module.

### check_tasksched

Check status of scheduled jobs.

* [Samples](#check_tasksched_samples)

* [Command-line Arguments](#check_tasksched_options)
* [Filter keywords](#check_tasksched_filter_keys)


<a name="check_tasksched_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckTaskSched_check_tasksched_samples.md)_

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_tasksched
/test: 1 != 0|'test'=1;0;0
```



<a name="check_tasksched_warn"/>

<a name="check_tasksched_crit"/>

<a name="check_tasksched_debug"/>

<a name="check_tasksched_show-all"/>

<a name="check_tasksched_escape-html"/>

<a name="check_tasksched_help"/>

<a name="check_tasksched_help-pb"/>

<a name="check_tasksched_show-default"/>

<a name="check_tasksched_help-short"/>

<a name="check_tasksched_force-old"/>

<a name="check_tasksched_computer"/>

<a name="check_tasksched_user"/>

<a name="check_tasksched_domain"/>

<a name="check_tasksched_password"/>

<a name="check_tasksched_folder"/>

<a name="check_tasksched_recursive"/>

<a name="check_tasksched_options"/>
#### Command-line Arguments


| Option                                          | Default Value                         | Description                                                                                                                            |
|-------------------------------------------------|---------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_tasksched_filter)               | enabled = 1                           | Filter which marks interesting items.                                                                                                  |
| [warning](#check_tasksched_warning)             | exit_code != 0                        | Filter which marks items which generates a warning state.                                                                              |
| warn                                            |                                       | Short alias for warning                                                                                                                |
| [critical](#check_tasksched_critical)           | exit_code < 0                         | Filter which marks items which generates a critical state.                                                                             |
| crit                                            |                                       | Short alias for critical.                                                                                                              |
| [ok](#check_tasksched_ok)                       |                                       | Filter which marks items which generates an ok state.                                                                                  |
| debug                                           | N/A                                   | Show debugging information in the log                                                                                                  |
| show-all                                        | N/A                                   | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                       |
| [empty-state](#check_tasksched_empty-state)     | warning                               | Return status to use when nothing matched filter.                                                                                      |
| [perf-config](#check_tasksched_perf-config)     |                                       | Performance data generation configuration                                                                                              |
| escape-html                                     | N/A                                   | Escape any < and > characters to prevent HTML encoding                                                                                 |
| help                                            | N/A                                   | Show help screen (this screen)                                                                                                         |
| help-pb                                         | N/A                                   | Show help screen as a protocol buffer payload                                                                                          |
| show-default                                    | N/A                                   | Show default values for a given command                                                                                                |
| help-short                                      | N/A                                   | Show help screen (short format).                                                                                                       |
| [top-syntax](#check_tasksched_top-syntax)       | ${status}: ${problem_list}            | Top level syntax.                                                                                                                      |
| [ok-syntax](#check_tasksched_ok-syntax)         | %(status): All tasks are ok           | ok syntax.                                                                                                                             |
| [empty-syntax](#check_tasksched_empty-syntax)   | %(status): No tasks found             | Empty syntax.                                                                                                                          |
| [detail-syntax](#check_tasksched_detail-syntax) | ${folder}/${title}: ${exit_code} != 0 | Detail level syntax.                                                                                                                   |
| [perf-syntax](#check_tasksched_perf-syntax)     | ${title}                              | Performance alias syntax.                                                                                                              |
| force-old                                       | N/A                                   | The name of the computer that you want to connect to.                                                                                  |
| computer                                        |                                       | The name of the computer that you want to connect to.                                                                                  |
| user                                            |                                       | The user name that is used during the connection to the computer.                                                                      |
| domain                                          |                                       | The domain of the user specified in the user parameter.                                                                                |
| password                                        |                                       | The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used. |
| folder                                          |                                       | The folder in which the tasks to check reside.                                                                                         |
| recursive                                       |                                       | Recurse sub folder (defaults to true).                                                                                                 |



<a name="check_tasksched_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


*Default Value:* | `enabled = 1`



<a name="check_tasksched_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



*Default Value:* | `exit_code != 0`



<a name="check_tasksched_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



*Default Value:* | `exit_code < 0`



<a name="check_tasksched_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_tasksched_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Default Value:* | `warning`



<a name="check_tasksched_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_tasksched_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Default Value:* | `${status}: ${problem_list}`



<a name="check_tasksched_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


*Default Value:* | `%(status): All tasks are ok`



<a name="check_tasksched_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


*Default Value:* | `%(status): No tasks found`



<a name="check_tasksched_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Default Value:* | `${folder}/${title}: ${exit_code} != 0`



<a name="check_tasksched_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Default Value:* | `${title}`




<a name="check_tasksched_filter_keys"/>
#### Filter keywords


| Option                                                        | Description                                                                                                  |
|---------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [application](#check_tasksched_application)                   | Retrieves the name of the application that the task is associated with.                                      |
| [comment](#check_tasksched_comment)                           | Retrieves the comment or description for the work item.                                                      |
| [count](#check_tasksched_count)                               | Number of items matching the filter. Common option for all checks.                                           |
| [creator](#check_tasksched_creator)                           | Retrieves the creator of the work item.                                                                      |
| [crit_count](#check_tasksched_crit_count)                     | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_tasksched_crit_list)                       | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_tasksched_detail_list)                   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [enabled](#check_tasksched_enabled)                           | TODO.                                                                                                        |
| [exit_code](#check_tasksched_exit_code)                       | Retrieves the work item's last exit code.                                                                    |
| [folder](#check_tasksched_folder)                             | The task folder                                                                                              |
| [has_run](#check_tasksched_has_run)                           | True if the task has ever executed.                                                                          |
| [list](#check_tasksched_list)                                 | A list of all items which matched the filter. Common option for all checks.                                  |
| [max_run_time](#check_tasksched_max_run_time)                 | Retrieves the maximum length of time the task can run.                                                       |
| [most_recent_run_time](#check_tasksched_most_recent_run_time) | Retrieves the most recent time the work item began running.                                                  |
| [ok_count](#check_tasksched_ok_count)                         | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_tasksched_ok_list)                           | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [parameters](#check_tasksched_parameters)                     | Retrieves the command-line parameters of a task.                                                             |
| [priority](#check_tasksched_priority)                         | Retrieves the priority for the task.                                                                         |
| [problem_count](#check_tasksched_problem_count)               | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_tasksched_problem_list)                 | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [status](#check_tasksched_status)                             | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [task_status](#check_tasksched_task_status)                   | Retrieves the status of the work item.                                                                       |
| [title](#check_tasksched_title)                               | The task title                                                                                               |
| [total](#check_tasksched_total)                               | Total number of items. Common option for all checks.                                                         |
| [warn_count](#check_tasksched_warn_count)                     | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_tasksched_warn_list)                       | A list of all items which matched the warning criteria. Common option for all checks.                        |
| [working_directory](#check_tasksched_working_directory)       | Retrieves the working directory of the task.                                                                 |


### checktasksched

Legacy version of check_tasksched


* [Command-line Arguments](#checktasksched_options)





<a name="checktasksched_help"/>

<a name="checktasksched_help-pb"/>

<a name="checktasksched_show-default"/>

<a name="checktasksched_help-short"/>

<a name="checktasksched_warn"/>

<a name="checktasksched_crit"/>

<a name="checktasksched_MaxWarn"/>

<a name="checktasksched_MaxCrit"/>

<a name="checktasksched_MinWarn"/>

<a name="checktasksched_MinCrit"/>

<a name="checktasksched_Counter"/>

<a name="checktasksched_truncate"/>

<a name="checktasksched_syntax"/>

<a name="checktasksched_master-syntax"/>

<a name="checktasksched_filter"/>

<a name="checktasksched_debug"/>

<a name="checktasksched_options"/>
#### Command-line Arguments


| Option        | Default Value | Description                                                     |
|---------------|---------------|-----------------------------------------------------------------|
| help          | N/A           | Show help screen (this screen)                                  |
| help-pb       | N/A           | Show help screen as a protocol buffer payload                   |
| show-default  | N/A           | Show default values for a given command                         |
| help-short    | N/A           | Show help screen (short format).                                |
| warn          |               | Warning bounds.                                                 |
| crit          |               | Critical bounds.                                                |
| MaxWarn       |               | Maximum value before a warning is returned.                     |
| MaxCrit       |               | Maximum value before a critical is returned.                    |
| MinWarn       |               | Minimum value before a warning is returned.                     |
| MinCrit       |               | Minimum value before a critical is returned.                    |
| Counter       |               | The time to check                                               |
| truncate      |               | Deprecated option                                               |
| syntax        |               | Syntax (same as detail-syntax in the check_tasksched check)     |
| master-syntax |               | Master Syntax (same as top-syntax in the check_tasksched check) |
| filter        |               | Filter (same as filter in the check_tasksched check)            |
| debug         | N/A           | Filter (same as filter in the check_tasksched check)            |






