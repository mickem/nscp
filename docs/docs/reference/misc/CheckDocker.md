# CheckDocker

Use this module to detect and monitor docker containers



## Enable module

To enable this module and and allow using the commands you need to ass `CheckDocker = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckDocker = enabled
```


## Queries

A quick reference for all available queries (check commands) in the CheckDocker module.

**List of commands:**

A list of all available queries (check commands)

| Command                       | Description                                      |
|-------------------------------|--------------------------------------------------|
| [check_docker](#check_docker) | Checks that a given docker container is running. |




### check_docker

Checks that a given docker container is running.


* [Command-line Arguments](#check_docker_options)
* [Filter keywords](#check_docker_filter_keys)





<a name="check_docker_warn"/>
<a name="check_docker_crit"/>
<a name="check_docker_debug"/>
<a name="check_docker_show-all"/>
<a name="check_docker_escape-html"/>
<a name="check_docker_help"/>
<a name="check_docker_help-pb"/>
<a name="check_docker_show-default"/>
<a name="check_docker_help-short"/>
<a name="check_docker_host"/>
<a name="check_docker_options"/>
#### Command-line Arguments


| Option                                       | Default Value                | Description                                                                                                      |
|----------------------------------------------|------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_docker_filter)               |                              | Filter which marks interesting items.                                                                            |
| [warning](#check_docker_warning)             | container_state != 'running' | Filter which marks items which generates a warning state.                                                        |
| warn                                         |                              | Short alias for warning                                                                                          |
| [critical](#check_docker_critical)           | container_state != 'running' | Filter which marks items which generates a critical state.                                                       |
| crit                                         |                              | Short alias for critical.                                                                                        |
| [ok](#check_docker_ok)                       |                              | Filter which marks items which generates an ok state.                                                            |
| debug                                        | N/A                          | Show debugging information in the log                                                                            |
| show-all                                     | N/A                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_docker_empty-state)     | warning                      | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_docker_perf-config)     |                              | Performance data generation configuration                                                                        |
| escape-html                                  | N/A                          | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                         | N/A                          | Show help screen (this screen)                                                                                   |
| help-pb                                      | N/A                          | Show help screen as a protocol buffer payload                                                                    |
| show-default                                 | N/A                          | Show default values for a given command                                                                          |
| help-short                                   | N/A                          | Show help screen (short format).                                                                                 |
| [top-syntax](#check_docker_top-syntax)       | ${status}: ${list}           | Top level syntax.                                                                                                |
| [ok-syntax](#check_docker_ok-syntax)         |                              | ok syntax.                                                                                                       |
| [empty-syntax](#check_docker_empty-syntax)   |                              | Empty syntax.                                                                                                    |
| [detail-syntax](#check_docker_detail-syntax) | ${names}=${container_state}  | Detail level syntax.                                                                                             |
| [perf-syntax](#check_docker_perf-syntax)     | ${id}                        | Performance alias syntax.                                                                                        |
| host                                         |                              | The host or socket of the docker deamon                                                                          |



<h5 id="check_docker_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_docker_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `container_state != 'running'`

<h5 id="check_docker_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `container_state != 'running'`

<h5 id="check_docker_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_docker_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `warning`

<h5 id="check_docker_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_docker_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_docker_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_docker_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_docker_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${names}=${container_state}`

<h5 id="check_docker_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${id}`


<a name="check_docker_filter_keys"/>
#### Filter keywords


| Option          | Description                                                                                                  |
|-----------------|--------------------------------------------------------------------------------------------------------------|
| command         | Command                                                                                                      |
| container_state | Container image                                                                                              |
| count           | Number of items matching the filter. Common option for all checks.                                           |
| crit_count      | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list       | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| id              | Container id                                                                                                 |
| image           | Container image                                                                                              |
| image_id        | Container image id                                                                                           |
| ip              | IP of container                                                                                              |
| list            | A list of all items which matched the filter. Common option for all checks.                                  |
| names           | Container image                                                                                              |
| ok_count        | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list         | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count   | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list    | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status          | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total           | Total number of items. Common option for all checks.                                                         |
| warn_count      | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list       | A list of all items which matched the warning criteria. Common option for all checks.                        |




