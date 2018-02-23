# Scheduler

Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA




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



## Configuration



| Path / Section                              | Description |
|---------------------------------------------|-------------|
| [/settings/scheduler](#scheduler)           | Scheduler   |
| [/settings/scheduler/schedules](#schedules) | Schedules   |



### Scheduler <a id="/settings/scheduler"/>

Section for the Scheduler module.




| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [threads](#threads) | 5             | Threads     |



```ini
# Section for the Scheduler module.
[/settings/scheduler]
threads=5

```





#### Threads <a id="/settings/scheduler/threads"></a>

Number of threads to use.





| Key            | Description                                 |
|----------------|---------------------------------------------|
| Path:          | [/settings/scheduler](#/settings/scheduler) |
| Key:           | threads                                     |
| Default value: | `5`                                         |
| Used by:       | Scheduler                                   |


**Sample:**

```
[/settings/scheduler]
# Threads
threads=5
```


### Schedules <a id="/settings/scheduler/schedules"/>

Section for the Scheduler module.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key         | Default Value | Description        |
|-------------|---------------|--------------------|
| alias       |               | ALIAS              |
| channel     |               | SCHEDULE CHANNEL   |
| command     |               | SCHEDULE COMMAND   |
| interval    |               | SCHEDULE INTERAVAL |
| is template | false         | IS TEMPLATE        |
| parent      | default       | PARENT             |
| randomness  |               | RANDOMNESS         |
| report      |               | REPORT MODE        |
| schedule    |               | SCHEDULE           |
| source      |               | SOURCE             |
| target      |               | TARGET             |


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
#schedule=...
#source=...
#target=...

```






