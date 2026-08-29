# Scheduler

Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA

## Enable module

To enable this module and and allow using the commands you need to ass `Scheduler = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
Scheduler = enabled
```

## Samples

_Feel free to add more samples [on this page](https://github.com/mickem/nscp/blob/master/docs/samples/Scheduler_samples.md)_

### Using the scheduler

The scheduler will run commands ate given intervals (or schedules) and submit the result to a module (such as NRDP or NSCA) which will submit the result to a remote monitoring server.
This is in the Nagios(TM) world referred to as passive monitoring.

A simple schedule looks like this:

```
[/settings/scheduler/schedules/default]
interval = 5s
channel = log

[/settings/scheduler/schedules]
eventlog=check_eventlog log=application
CPU Load=check_cpu
```

The above configuration use the inherited default section to set some defaults and then adds two schedules this is convenient if you want to add many schedules which are similar.
It is identical to the following:

```
[/settings/scheduler/schedules/default]
interval = 5s
channel = log

[/settings/scheduler/schedules/eventlog]
command=check_eventlog log=application

[/settings/scheduler/schedules/CPU Load]
command=check_cpu
```

#### Interval or schedules?

There are two way to schedule things:

#. interval - The command will be run at around ever x seconds (or minutes)
#. schedule - The command will be executed at given times.

The interval is the normal scenario if you do not care when something ix executed as long as it is reoccurring. You can even set randomness to make sure the time is random.
The reason shy most people want this is that it makes the network load more even. If you schedule your entire server infrastructure to run the checks at 13:00 you will have a network spike.

The following example use interval to schedule the check ever 10 minutes:

```
[/settings/scheduler/schedules/CPU Load]
interval = 5s
channel = log
command=check_cpu
```

The following does the same using schedule instead:

```
[/settings/scheduler/schedules/CPU Load]
schedule=0,10,20,30,50 * * * *
channel = log
command=check_cpu
```

The syntax of the schedule is similar to a cron expression in that you have:

| Name         | Allowed Values | Allowed Special Characters |
|--------------|----------------|----------------------------|
| Seconds      | 0-59           | , *                        |
| Minutes      | 0-59           | , *                        |
| Hours        | 0-23           | , *                        |
| Day of month | 1-31           | , *                        |
| Month        | 0-11           | , *                        |
| Day of week  | 1-7            | , *                        |

#### Running a check at startup

Both interval and schedule only report for the first time once the interval (or
the next matching time) has elapsed. With a long interval that leaves the
monitoring server with the old result for a long time after a reboot - exactly
when the status is most likely to have changed. Set `run on startup` to run the
command once as soon as the agent has started:

```
[/settings/scheduler/schedules/uptime]
interval = 1h
channel = NSCA
command = check_uptime
run on startup = true
```

The schedule is otherwise unaffected: the next run follows an hour after the
startup run. The startup run also happens after a configuration reload, so a
schedule you just changed reports its new status right away.

Setting it on the default section turns it on for every schedule which does not
override it:

```
[/settings/scheduler/schedules/default]
interval = 1h
channel = NSCA
run on startup = true
```

If you have a lot of schedules and do not want all of them to report at the very
same instant, spread the startup runs out over a window:

```
[/settings/scheduler]
startup window = 30s
```


## Queries

A quick reference for all available queries (check commands) in the Scheduler module.

**List of commands:**

A list of all available queries (check commands)

| Command                         | Description                                                                                                  |
|---------------------------------|--------------------------------------------------------------------------------------------------------------|
| [run_schedules](#run_schedules) | Run configured schedules now instead of waiting for their interval and submit the results as passive checks. |

### run_schedules

Run configured schedules now instead of waiting for their interval and submit the results as passive checks.

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

**Jump to section:**

* [Sample Commands](#run_schedules_samples)
* [Command-line Arguments](#run_schedules_options)


<a id="run_schedules_samples"></a>
#### Sample Commands

Given this configuration:

```ini
[/modules]
CheckSystem = enabled
CheckHelpers = enabled
Scheduler = enabled
NSCAClient = enabled

[/settings/scheduler/schedules/default]
channel  = NSCA
interval = 1h
report   = all

[/settings/scheduler/schedules]
cpu        = check_cpu
host_check = check_ok
```

**Run every configured schedule now and submit the results:**

```
nscp client --boot --query run_schedules
Ran 2 schedule(s): cpu, host_check
```

**Run a single schedule (repeat `schedule=` for more than one):**

```
nscp client --boot --query run_schedules schedule=cpu
Ran 1 schedule(s): cpu
```

The `-a` / `--argument` spelling works too, which is what you want from a batch
file or a script:

```
nscp client --boot --query run_schedules --argument schedule=cpu
Ran 1 schedule(s): cpu
```

**A schedule that does not exist is reported, not silently skipped:**

```
nscp client --boot --query run_schedules schedule=nosuchcheck
No such schedule: nosuchcheck (available: cpu, host_check)
```

**From the interactive test prompt:**

```
nscp test
...
run_schedules
OK: Ran 2 schedule(s): cpu, host_check
```

**Over REST**, to wire a "push my results now" button to the agent:

```
curl -k -u admin:<password> "https://<agent>:8443/api/v2/queries/run_schedules/commands/execute?schedule=cpu"
{"command":"run_schedules","result":0,"lines":[{"message":"Ran 1 schedule(s): cpu","perf":{}}]}
```

**Via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command run_schedules
OK: Ran 2 schedule(s): cpu, host_check
```



<a id="run_schedules_options"></a>
#### Command-line Arguments

<a id="run_schedules_schedule"></a>

| Option   | Default Value | Description                                                                                     |
|----------|---------------|-------------------------------------------------------------------------------------------------|
| schedule |               | Alias of a schedule to run, can be given more than once. Defaults to every configured schedule. |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                              | Description |
|---------------------------------------------|-------------|
| [/settings/scheduler](#scheduler)           | Scheduler   |
| [/settings/scheduler/schedules](#schedules) | Schedules   |


### Scheduler <a id="/settings/scheduler"></a>

Section for the Scheduler module.

| Key                               | Default Value | Description    |
|-----------------------------------|---------------|----------------|
| [startup window](#startup-window) | 0s            | Startup window |
| [threads](#threads)               | 5             | Threads        |
| [timezone](#timezone)             | local         | Timezone       |


```ini
# Section for the Scheduler module.
[/settings/scheduler]
startup window=0s
threads=5
timezone=local
```

#### Startup window <a id="/settings/scheduler/startup window"></a>

Time over which schedules with 'run on startup' are spread out when the agent starts. The default (0s) runs them all immediately; raise it if you have many startup schedules and do not want to hit the monitoring server with all of them at once.


| Key            | Description                                 |
|----------------|---------------------------------------------|
| Path:          | [/settings/scheduler](#/settings/scheduler) |
| Key:           | startup window                              |
| Default value: | `0s`                                        |


**Sample:**

```
[/settings/scheduler]
# Startup window
startup window=0s
```

#### Threads <a id="/settings/scheduler/threads"></a>

Number of threads to use.


| Key            | Description                                 |
|----------------|---------------------------------------------|
| Path:          | [/settings/scheduler](#/settings/scheduler) |
| Key:           | threads                                     |
| Default value: | `5`                                         |


**Sample:**

```
[/settings/scheduler]
# Threads
threads=5
```

#### Timezone <a id="/settings/scheduler/timezone"></a>

Reference clock for cron expressions. Accepts 'local' (default — standard cron semantics), 'utc'/'gmt' (restores the pre-0.13 behaviour), or any POSIX TZ string such as 'EST-05EDT,M3.2.0,M11.1.0'. Unparseable values fall back to UTC and are flagged in the tz label as 'UTC?'.


| Key            | Description                                 |
|----------------|---------------------------------------------|
| Path:          | [/settings/scheduler](#/settings/scheduler) |
| Key:           | timezone                                    |
| Default value: | `local`                                     |


**Sample:**

```
[/settings/scheduler]
# Timezone
timezone=local
```

### Schedules <a id="/settings/scheduler/schedules"></a>

Section for the Scheduler module.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key            | Default Value | Description       |
|----------------|---------------|-------------------|
| alias          |               | ALIAS             |
| channel        |               | SCHEDULE CHANNEL  |
| command        |               | SCHEDULE COMMAND  |
| interval       |               | SCHEDULE INTERVAL |
| is template    | false         | IS TEMPLATE       |
| parent         | default       | PARENT            |
| randomness     |               | RANDOMNESS        |
| report         |               | REPORT MODE       |
| run on startup |               | RUN ON STARTUP    |
| schedule       |               | SCHEDULE          |
| source         |               | SOURCE            |
| target         |               | TARGET            |


**Sample:**

```ini
# An example of a Schedules section
[/settings/scheduler/schedules/sample]
#alias=...
#channel=...
#command=...
#interval=...
is template=false
parent=default
#randomness=...
#report=...
#run on startup=...
#schedule=...
#source=...
#target=...

```





