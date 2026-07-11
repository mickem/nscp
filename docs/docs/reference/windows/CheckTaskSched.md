# CheckTaskSched

*Available on Windows only.*

Check status of your scheduled jobs.

## Enable module

To enable this module and and allow using the commands you need to ass `CheckTaskSched = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckTaskSched = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckTaskSched module.

**List of commands:**

A list of all available queries (check commands)

| Command                             | Description                     |
|-------------------------------------|---------------------------------|
| [check_tasksched](#check_tasksched) | Check status of scheduled jobs. |

### check_tasksched

Check status of scheduled jobs.

Checks the state of Windows Scheduled Tasks. Uses the modern Task Scheduler 2.0
API (`IRegisteredTask`) by default, falling back to the legacy `ITask` API on
downlevel systems or when `force-old=true`.

### Run-time keywords

| Keyword                 | Type  | Description                                                                                                                                              |
|-------------------------|-------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| `most_recent_run_time`  | date  | The most recent time the task began running. Comparable to relative times, e.g. `most_recent_run_time < -1d`.                                            |
| `has_run`               | bool  | True if the task has ever executed.                                                                                                                      |
| `next_run_time`         | date  | The next time the task is scheduled to run. Rendered as `none` (value `0`) when the task has no upcoming run (disabled, on-demand, or no more triggers). |
| `number_of_missed_runs` | int   | Number of times the task was scheduled to run but did not. Always `0` on the legacy `ITask` API.                                                         |
| `last_run_age`          | int   | Seconds since the task last ran, or `-1` if it has never run. Convenient for stale-task alerts, e.g. `last_run_age > 86400`.                             |
| `task_status`           | state | The task state (`ready`, `running`, `disabled`, `queued`, `unknown`).                                                                                    |
| `exit_code`             | int   | The task's last run result (`lasttaskresult`).                                                                                                           |

### Default performance data

`check_tasksched` emits `task_status` (state), `number_of_missed_runs`
(missedruns), and `exit_code` (lasttaskresult) as perfdata by default, one set
per matched task. Suppress with `perf-config=extra()` / `perf-syntax=none`, or
narrow the matched set with `filter=`.

**Jump to section:**

* [Sample Commands](#check_tasksched_samples)
* [Command-line Arguments](#check_tasksched_options)
* [Filter keywords](#check_tasksched_filter_keys)


<a id="check_tasksched_samples"></a>
#### Sample Commands

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_tasksched
/test: 1 != 0|'test'=1;0;0
```

**Alerting on stale tasks (last run older than a day):**

`last_run_age` is the seconds since the task last ran (`-1` if it has never run),
so you can alert on tasks that should be running regularly but have gone quiet.

```
check_tasksched "filter=title = 'Backup'" "crit=last_run_age > 86400" "detail-syntax=${title}: last ran ${most_recent_run_time}"
CRITICAL: \Backup: last ran 2026-07-04 02:00:00
```

**Alerting on missed runs and inspecting the next scheduled run:**

`number_of_missed_runs` and `next_run_time` come from the modern Task Scheduler
API. Both are also emitted as perfdata by default (alongside `task_status` state
and the `exit_code` last-run result).

```
check_tasksched "filter=folder = '\\'" "warn=number_of_missed_runs > 0" "detail-syntax=${title}: ${number_of_missed_runs} missed, next ${next_run_time}"
WARNING: \DailyReport: 2 missed, next 2026-07-07 06:00:00
'DailyReport task_status'=3;;; 'DailyReport number_of_missed_runs'=2;0;; ...
```

Equivalent semantics also work against `most_recent_run_time` directly — the
where-parser understands relative-time thresholds, so
`crit=most_recent_run_time < -1d` means "last run older than a day".



<a id="check_tasksched_options"></a>
#### Command-line Arguments

<a id="check_tasksched_warn"></a>
<a id="check_tasksched_crit"></a>
<a id="check_tasksched_debug"></a>
<a id="check_tasksched_show-all"></a>
<a id="check_tasksched_escape-html"></a>
<a id="check_tasksched_help"></a>
<a id="check_tasksched_help-pb"></a>
<a id="check_tasksched_show-default"></a>
<a id="check_tasksched_help-short"></a>
<a id="check_tasksched_computer"></a>
<a id="check_tasksched_user"></a>
<a id="check_tasksched_domain"></a>
<a id="check_tasksched_password"></a>
<a id="check_tasksched_folder"></a>
<a id="check_tasksched_recursive"></a>
<a id="check_tasksched_hidden"></a>

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
| [force-old](#check_tasksched_force-old)         | 1)] (=0                               | The name of the computer that you want to connect to.                                                                                  |
| computer                                        |                                       | The name of the computer that you want to connect to.                                                                                  |
| user                                            |                                       | The user name that is used during the connection to the computer.                                                                      |
| domain                                          |                                       | The domain of the user specified in the user parameter.                                                                                |
| password                                        |                                       | The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used. |
| folder                                          |                                       | The folder in which the tasks to check reside.                                                                                         |
| recursive                                       |                                       | Recurse sub folder (defaults to true).                                                                                                 |
| hidden                                          |                                       | Look for hidden tasks.                                                                                                                 |



<h5 id="check_tasksched_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `enabled = 1`

<h5 id="check_tasksched_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `exit_code != 0`

<h5 id="check_tasksched_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `exit_code < 0`

<h5 id="check_tasksched_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_tasksched_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `warning`

<h5 id="check_tasksched_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_tasksched_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_tasksched_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All tasks are ok`

<h5 id="check_tasksched_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No tasks found`

<h5 id="check_tasksched_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${folder}/${title}: ${exit_code} != 0`

<h5 id="check_tasksched_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${title}`

<h5 id="check_tasksched_force-old">force-old:</h5>

The name of the computer that you want to connect to.

*Default Value:* `1)] (=0`


<a id="check_tasksched_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                             |
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

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |

