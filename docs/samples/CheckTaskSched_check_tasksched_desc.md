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
