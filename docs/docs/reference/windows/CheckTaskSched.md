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

#### Default performance data

`check_tasksched` emits `task_status` (state), `number_of_missed_runs`
(missedruns), and `exit_code` (lasttaskresult) as perfdata by default, one set
per matched task. Suppress with `perf-config=extra()` / `perf-syntax=none`, or
narrow the matched set with `filter=`.

#### Hidden tasks

Tasks marked *Hidden* (the `ITaskSettings` Hidden flag) are **excluded from
enumeration by default** — pass `hidden=true` to include them. The `hidden`
keyword then reports each task's flag, so `hidden=true "filter=hidden = 1"`
lists only the hidden tasks. Without `hidden=true` a hidden task is invisible to
the check regardless of any `hidden` reference in the filter.

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

**Reporting the task path (`uri`) and listing hidden tasks:**

`uri` is the task's full path — the same identifier the Task Scheduler UI and
`schtasks` use. Hidden tasks are skipped unless `hidden=true` is passed; the
`hidden` keyword then reports the flag.

```
check_tasksched hidden=true "filter=hidden = 1" "top-syntax=${count} hidden tasks" "detail-syntax=${uri}"
OK: 7 hidden tasks
```

```
check_tasksched "filter=title = 'ScheduledDefrag'" "detail-syntax=${uri} hidden=${hidden}"
OK: \Microsoft\Windows\Defrag\ScheduledDefrag hidden=0
```



<a id="check_tasksched_options"></a>
#### Command-line Arguments

<a id="check_tasksched_computer"></a>
<a id="check_tasksched_user"></a>
<a id="check_tasksched_domain"></a>
<a id="check_tasksched_password"></a>
<a id="check_tasksched_folder"></a>
<a id="check_tasksched_recursive"></a>
<a id="check_tasksched_hidden"></a>

| Option                                  | Default Value | Description                                                                                                                            |
|-----------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------|
| [force-old](#check_tasksched_force-old) | false         | The name of the computer that you want to connect to.                                                                                  |
| computer                                |               | The name of the computer that you want to connect to.                                                                                  |
| user                                    |               | The user name that is used during the connection to the computer.                                                                      |
| domain                                  |               | The domain of the user specified in the user parameter.                                                                                |
| password                                |               | The password that is used to connect to the computer. If the user name and password are not specified, then the current token is used. |
| folder                                  |               | The folder in which the tasks to check reside.                                                                                         |
| recursive                               |               | Recurse sub folder (defaults to true).                                                                                                 |
| hidden                                  |               | Look for hidden tasks.                                                                                                                 |



<h5 id="check_tasksched_force-old">force-old:</h5>

The name of the computer that you want to connect to.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                           | Default Value                         |
|--------------------------------------------------------------------------------------------------|---------------------------------------|
| <a id="check_tasksched_filter"></a>[filter](../common-options.md#filter)                         | enabled = 1                           |
| <a id="check_tasksched_warning"></a>[warning](../common-options.md#warning)                      | exit_code != 0                        |
| <a id="check_tasksched_warn"></a>[warn](../common-options.md#warn)                               |                                       |
| <a id="check_tasksched_critical"></a>[critical](../common-options.md#critical)                   | exit_code < 0                         |
| <a id="check_tasksched_crit"></a>[crit](../common-options.md#crit)                               |                                       |
| <a id="check_tasksched_ok"></a>[ok](../common-options.md#ok)                                     |                                       |
| <a id="check_tasksched_debug"></a>[debug](../common-options.md#debug)                            | false                                 |
| <a id="check_tasksched_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                 |
| <a id="check_tasksched_empty-state"></a>[empty-state](../common-options.md#empty-state)          | warning                               |
| <a id="check_tasksched_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                       |
| <a id="check_tasksched_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                 |
| <a id="check_tasksched_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                     |
| <a id="check_tasksched_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${problem_list}            |
| <a id="check_tasksched_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                | %(status): All tasks are ok           |
| <a id="check_tasksched_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No tasks found             |
| <a id="check_tasksched_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${folder}/${title}: ${exit_code} != 0 |
| <a id="check_tasksched_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${title}                              |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_tasksched_filter_keys"></a>
#### Filter keywords

| Option                | Description                                                                                                                                                                              |
|-----------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| application           | Retrieves the name of the application that the task is associated with.                                                                                                                  |
| comment               | Retrieves the comment or description for the work item.                                                                                                                                  |
| creator               | Retrieves the creator of the work item.                                                                                                                                                  |
| enabled               | TODO.                                                                                                                                                                                    |
| exit_code             | The task's last run result (last exit code).                                                                                                                                             |
| folder                | The task folder                                                                                                                                                                          |
| has_run               | True if the task has ever executed.                                                                                                                                                      |
| hidden                | True if the task is marked hidden (ITaskSettings Hidden flag). Always false on the legacy ITask API.                                                                                     |
| last_run_age          | Seconds since the task last ran (-1 if it has never run). Use e.g. last_run_age > 86400 to alert on stale tasks.                                                                         |
| max_run_time          | Retrieves the maximum length of time the task can run.                                                                                                                                   |
| most_recent_run_time  | The most recent time the task began running. Comparable to relative times, e.g. most_recent_run_time < -1d.                                                                              |
| next_run_time         | The next time the task is scheduled to run. Rendered as 'none' (value 0) when the task has no upcoming run (disabled, on-demand, or no more triggers).                                   |
| number_of_missed_runs | Number of times the task was scheduled to run but did not (0 on the legacy ITask API).                                                                                                   |
| parameters            | Retrieves the command-line parameters of a task.                                                                                                                                         |
| priority              | Retrieves the priority for the task.                                                                                                                                                     |
| task_status           | The task state: ready, running, disabled, queued or unknown (the legacy ITask API instead uses ready, running, disabled, not_scheduled, has_not_run, no_more_runs or no_valid_triggers). |
| title                 | The task title                                                                                                                                                                           |
| uri                   | The task's full path / URI (e.g. \Microsoft\Windows\Defrag\ScheduledDefrag). Empty on the legacy ITask API.                                                                              |
| working_directory     | Retrieves the working directory of the task.                                                                                                                                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

