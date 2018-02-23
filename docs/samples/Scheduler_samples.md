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
