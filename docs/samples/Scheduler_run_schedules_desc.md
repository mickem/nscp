#### About `run_schedules`

`run_schedules` runs the schedules configured under
`[/settings/scheduler/schedules]` **now**, instead of waiting for their interval
(or cron expression) to come around, and submits the results on their normal
channel.

This is what you want after editing `nsclient.ini`: change a check's arguments,
reload, run `run_schedules`, and the new result is on the monitoring server
within seconds rather than at the end of the interval.

Each schedule is executed exactly as the scheduler itself would: the command
runs, the `report` filter decides whether the result is worth sending, and the
result is submitted on the schedule's `channel` with its `target`, `source` and
alias. A schedule with `channel = drop`, or one whose result the `report` filter
rejects, therefore sends nothing here either — and is still counted as run.

| Option     | What it is for                                                                                   |
|------------|---------------------------------------------------------------------------------------------------|
| `schedule` | Alias of a schedule to run, repeat for more than one. Defaults to every configured schedule.       |

The command returns **OK** when every selected schedule ran and submitted, and
**UNKNOWN** when a schedule was not found, when none is configured, or when a
submission failed (the message names the schedule and the error). It does not
return the status of the checks themselves — those go to the monitoring server.

The run is synchronous: the command answers once every selected schedule has
finished, so running all of them at once takes as long as the slowest check.
It also does not touch the timers — the next regular run of each schedule
happens exactly when it would have anyway.

!!! note

    The checks run with the permissions of whoever called `run_schedules`, not
    with those of the Scheduler module — see
    [permissions](../../concepts/permissions.md). A schedule whose check is
    denied submits nothing to its channel and is reported back as failed. The
    scheduler's own timed runs are unaffected and keep running as the
    Scheduler.

!!! tip

    To have the schedules run at startup instead — for instance so results are
    fresh after a reboot or a service restart — set `run on startup = true` on
    the schedule rather than calling this command.
