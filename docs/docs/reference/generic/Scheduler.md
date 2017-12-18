# Scheduler

Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA







**Configuration Keys:**



    
    
| Path / Section                              | Key                                     | Description |
|---------------------------------------------|-----------------------------------------|-------------|
| [/settings/scheduler](#/settings/scheduler) | [threads](#/settings/scheduler_threads) | Threads     |


| Path / Section                                                  | Description |
|-----------------------------------------------------------------|-------------|
| [/settings/scheduler/schedules](#/settings/scheduler/schedules) | Schedules   |





## Configuration

<a name="/settings/scheduler"/>
### Scheduler

Section for the Scheduler module.




| Key                                     | Default Value | Description |
|-----------------------------------------|---------------|-------------|
| [threads](#/settings/scheduler_threads) | 5             | Threads     |



```ini
# Section for the Scheduler module.
[/settings/scheduler]
threads=5

```




<a name="/settings/scheduler_threads"/>

**Threads**

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


<a name="/settings/scheduler/schedules"/>
### Schedules

Section for the Scheduler module.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key         | Default Value | Description        |
|-------------|---------------|--------------------|
| alias       |               | ALIAS              |
| channel     |               | SCHEDULE CHANNEL   |
| command     |               | SCHEDULE COMMAND   |
| interval    |               | SCHEDULE INTERVAL  |
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






