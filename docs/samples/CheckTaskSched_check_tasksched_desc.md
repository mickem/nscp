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
