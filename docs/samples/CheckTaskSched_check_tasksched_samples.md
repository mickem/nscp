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
