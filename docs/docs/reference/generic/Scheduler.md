# Scheduler

Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA







## List of Configuration


### Common Keys

| Path / Section                              | Key                                     | Description  |
|---------------------------------------------|-----------------------------------------|--------------|
| [/settings/scheduler](#/settings/scheduler) | [threads](#/settings/scheduler_threads) | THREAD COUNT |







# Configuration

<a name="/settings/scheduler"/>
## SCHEDULER SECTION

Section for the Scheduler module.

```ini
# Section for the Scheduler module.
[/settings/scheduler]
threads=5

```


| Key                                     | Default Value | Description  |
|-----------------------------------------|---------------|--------------|
| [threads](#/settings/scheduler_threads) | 5             | THREAD COUNT |




<a name="/settings/scheduler_threads"/>
### threads

**THREAD COUNT**

Number of threads to use.




| Key            | Description                                 |
|----------------|---------------------------------------------|
| Path:          | [/settings/scheduler](#/settings/scheduler) |
| Key:           | threads                                     |
| Default value: | `5`                                         |
| Used by:       | Scheduler                                   |


#### Sample

```
[/settings/scheduler]
# THREAD COUNT
threads=5
```


<a name="/settings/scheduler/schedules"/>
## SCHEDULER SECTION

Section for the Scheduler module.

```ini
# Section for the Scheduler module.
[/settings/scheduler/schedules]

```






