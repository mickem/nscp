# CheckExternalScripts

Module used to execute external scripts

## Description

`CheckExternalScripts` is used to run scripts and programs you provide your self as opposed to internal commands provided by modules and internal scripts. You can also fond many third part generated scripts at various sites:

*   [Nagios Exchange](https://exchange.nagios.org/)
*   [Icinga Exchange](https://exchange.icinga.com/)

To use this module you need to enable it like so:

```
nscp settings --activate-module CheckExternalScripts
```

Which will add the following to your configuration:

```
[/modules]
CheckExternalScripts = enabled
```

There is an extensive guide on using external scripts with NSClient++ [here](../../howto/external_scripts.md) as well as some examples in the [samples section](#samples) of this page.




## List of command aliases

A list of all short hand aliases for queries (check commands)


| Command               | Description                         |
|-----------------------|-------------------------------------|
| alias_cpu             | Alias for: :query:`check_cpu`       |
| alias_cpu_ex          | Alias for: :query:`check_cpu`       |
| alias_disk            | Alias for: :query:`check_drivesize` |
| alias_disk_loose      | Alias for: :query:`check_drivesize` |
| alias_event_log       | Alias for: :query:`check_eventlog`  |
| alias_file_age        | Alias for: :query:`check_files`     |
| alias_file_size       | Alias for: :query:`check_files`     |
| alias_mem             | Alias for: :query:`check_memory`    |
| alias_process         | Alias for: :query:`check_process`   |
| alias_process_count   | Alias for: :query:`check_process`   |
| alias_process_hung    | Alias for: :query:`check_process`   |
| alias_process_stopped | Alias for: :query:`check_process`   |
| alias_sched_all       | Alias for: :query:`check_tasksched` |
| alias_sched_long      | Alias for: :query:`check_tasksched` |
| alias_sched_task      | Alias for: :query:`check_tasksched` |
| alias_service         | Alias for: :query:`check_service`   |
| alias_service_ex      | Alias for: :query:`check_service`   |
| alias_up              | Alias for: :query:`check_uptime`    |
| alias_volumes         | Alias for: :query:`check_drivesize` |
| alias_volumes_loose   | Alias for: :query:`check_drivesize` |


## List of Configuration


### Common Keys

| Path / Section                                                                            | Key                                                                              | Description                                                 |
|-------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------|-------------------------------------------------------------|
| [/settings/external scripts](#/settings/external scripts)                                 | [allow arguments](#/settings/external scripts_allow arguments)                   | Allow arguments when executing external scripts             |
| [/settings/external scripts](#/settings/external scripts)                                 | [allow nasty characters](#/settings/external scripts_allow nasty characters)     | Allow certain potentially dangerous characters in arguments |
| [/settings/external scripts](#/settings/external scripts)                                 | [script path](#/settings/external scripts_script path)                           | Load all scripts in a given folder                          |
| [/settings/external scripts](#/settings/external scripts)                                 | [script root](#/settings/external scripts_script root)                           | Script root folder                                          |
| [/settings/external scripts](#/settings/external scripts)                                 | [timeout](#/settings/external scripts_timeout)                                   | Command timeout                                             |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_cpu](#/settings/external scripts/alias_alias_cpu)                         | alias_cpu                                                   |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_cpu_ex](#/settings/external scripts/alias_alias_cpu_ex)                   | alias_cpu_ex                                                |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_disk](#/settings/external scripts/alias_alias_disk)                       | alias_disk                                                  |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_disk_loose](#/settings/external scripts/alias_alias_disk_loose)           | alias_disk_loose                                            |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_event_log](#/settings/external scripts/alias_alias_event_log)             | alias_event_log                                             |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_file_age](#/settings/external scripts/alias_alias_file_age)               | alias_file_age                                              |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_file_size](#/settings/external scripts/alias_alias_file_size)             | alias_file_size                                             |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_mem](#/settings/external scripts/alias_alias_mem)                         | alias_mem                                                   |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_process](#/settings/external scripts/alias_alias_process)                 | alias_process                                               |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_process_count](#/settings/external scripts/alias_alias_process_count)     | alias_process_count                                         |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_process_hung](#/settings/external scripts/alias_alias_process_hung)       | alias_process_hung                                          |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_process_stopped](#/settings/external scripts/alias_alias_process_stopped) | alias_process_stopped                                       |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_sched_all](#/settings/external scripts/alias_alias_sched_all)             | alias_sched_all                                             |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_sched_long](#/settings/external scripts/alias_alias_sched_long)           | alias_sched_long                                            |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_sched_task](#/settings/external scripts/alias_alias_sched_task)           | alias_sched_task                                            |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_service](#/settings/external scripts/alias_alias_service)                 | alias_service                                               |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_service_ex](#/settings/external scripts/alias_alias_service_ex)           | alias_service_ex                                            |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_up](#/settings/external scripts/alias_alias_up)                           | alias_up                                                    |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_volumes](#/settings/external scripts/alias_alias_volumes)                 | alias_volumes                                               |
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | [alias_volumes_loose](#/settings/external scripts/alias_alias_volumes_loose)     | alias_volumes_loose                                         |
| [/settings/external scripts/alias/default](#/settings/external scripts/alias/default)     | [command](#/settings/external scripts/alias/default_command)                     | COMMAND                                                     |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [command](#/settings/external scripts/scripts/default_command)                   | COMMAND                                                     |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [ignore perfdata](#/settings/external scripts/scripts/default_ignore perfdata)   | IGNORE PERF DATA                                            |
| [/settings/external scripts/wrappings](#/settings/external scripts/wrappings)             | [bat](#/settings/external scripts/wrappings_bat)                                 | Batch file                                                  |
| [/settings/external scripts/wrappings](#/settings/external scripts/wrappings)             | [ps1](#/settings/external scripts/wrappings_ps1)                                 | POWERSHELL WRAPPING                                         |
| [/settings/external scripts/wrappings](#/settings/external scripts/wrappings)             | [vbs](#/settings/external scripts/wrappings_vbs)                                 | Visual basic script                                         |

### Advanced keys

| Path / Section                                                                            | Key                                                                          | Description    |
|-------------------------------------------------------------------------------------------|------------------------------------------------------------------------------|----------------|
| [/settings/external scripts/alias/default](#/settings/external scripts/alias/default)     | [alias](#/settings/external scripts/alias/default_alias)                     | ALIAS          |
| [/settings/external scripts/alias/default](#/settings/external scripts/alias/default)     | [is template](#/settings/external scripts/alias/default_is template)         | IS TEMPLATE    |
| [/settings/external scripts/alias/default](#/settings/external scripts/alias/default)     | [parent](#/settings/external scripts/alias/default_parent)                   | PARENT         |
| [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample)       | [alias](#/settings/external scripts/alias/sample_alias)                      | ALIAS          |
| [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample)       | [is template](#/settings/external scripts/alias/sample_is template)          | IS TEMPLATE    |
| [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample)       | [parent](#/settings/external scripts/alias/sample_parent)                    | PARENT         |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [alias](#/settings/external scripts/scripts/default_alias)                   | ALIAS          |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [capture output](#/settings/external scripts/scripts/default_capture output) | CAPTURE OUTPUT |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [display](#/settings/external scripts/scripts/default_display)               | DISPLAY        |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [domain](#/settings/external scripts/scripts/default_domain)                 | DOMAIN         |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [encoding](#/settings/external scripts/scripts/default_encoding)             | ENCODING       |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [is template](#/settings/external scripts/scripts/default_is template)       | IS TEMPLATE    |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [parent](#/settings/external scripts/scripts/default_parent)                 | PARENT         |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [password](#/settings/external scripts/scripts/default_password)             | PASSWORD       |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [session](#/settings/external scripts/scripts/default_session)               | SESSION        |
| [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) | [user](#/settings/external scripts/scripts/default_user)                     | USER           |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample)   | [alias](#/settings/external scripts/scripts/sample_alias)                    | ALIAS          |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample)   | [is template](#/settings/external scripts/scripts/sample_is template)        | IS TEMPLATE    |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample)   | [parent](#/settings/external scripts/scripts/sample_parent)                  | PARENT         |

### Sample keys

| Path / Section                                                                          | Key                                                                           | Description      |
|-----------------------------------------------------------------------------------------|-------------------------------------------------------------------------------|------------------|
| [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample)     | [command](#/settings/external scripts/alias/sample_command)                   | COMMAND          |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [capture output](#/settings/external scripts/scripts/sample_capture output)   | CAPTURE OUTPUT   |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [command](#/settings/external scripts/scripts/sample_command)                 | COMMAND          |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [display](#/settings/external scripts/scripts/sample_display)                 | DISPLAY          |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [domain](#/settings/external scripts/scripts/sample_domain)                   | DOMAIN           |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [encoding](#/settings/external scripts/scripts/sample_encoding)               | ENCODING         |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [ignore perfdata](#/settings/external scripts/scripts/sample_ignore perfdata) | IGNORE PERF DATA |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [password](#/settings/external scripts/scripts/sample_password)               | PASSWORD         |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [session](#/settings/external scripts/scripts/sample_session)                 | SESSION          |
| [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) | [user](#/settings/external scripts/scripts/sample_user)                       | USER             |



# Usage

_To edit the usage section please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckExternalScripts_samples.md)_

## Adding a simple script

Adding a script we ca use the short hand format:

```
[/settings/external scripts/scripts]
my_ok1 = scripts\check_ok.bat
my_ok2 = scripts\check_ok.bat
```

Or the long format:

```
[/settings/external scripts/scripts/my_ok1]
command = scripts\check_ok.bat

[/settings/external scripts/scripts/my_ok2]
command = scripts\check_ok.bat
```

There is no difference between the two formats.
Both will add two new commands called my_ok1 and my_ok2 which in turn will execute the scripts\check_ok.bat script.
Thus for most cases the short hand is preferred (and most commonly used).
The reason for the long format is when you need to customize your command.
There are a number of options which can be set to customize the command: for instance which user should run the command.
These cannot be set using the short format.

## Using arguments

There are two ways to use arguments.

1.  Hardcoded into the command
2.  Allowing argument-pass through

The first option (hard-coding them) is obviously the more secure option as a third party cannot provide his or her own arguments.
But it adds to the maintenance burden as whenever you want to change an option you need to update the NSClient++ configuration (something which can be costly if you have many servers).

To allow argument pass-through you need to set:

```
[/settings/external scripts]
allow arguments = true
```

Please note when it comes to arguments they can (and often need to) be configured in two place.
Once for the NRPE Server and once for `CheckExternalScripts`.

## Running a command as a user

Running a command as a given user (to use elevated privileges for instance) you need to use the long format:

```
[/settings/external scripts/scripts/check_as_user]
command = scripts\check_ok.bat
user = Administrator
password = 1qflkasdhf7ejd8/kjhskjhk(/)"#
```

You can also specify a session and to show the output if you want to have the program visible:

```
[/settings/external scripts/scripts/annoy_users]
command = notepad.exe
session = 1
display = true
```

## Programs "running forever"

Another use case of external scripts is to have event handlers which starts programs.
This is trickier then it sounds because all commands have a timeout and once that is reach they are killed.
NSClient++ exits it also terminates all running script thus your "fix" will not be very long.

To work around this you need to start the program without the control of NSClient++ (fork). 
To do this you need to set capture output to false like so:

```
[/settings/external scripts/scripts/fix_problem]
command = notepad.exe
capture output = false
```

The draw back to this is that the script cannot return any output neither message nor status code.

!!! danger
    A word of warning using "start" or other similar measure to try to start a program in a regular script will cause a rather nasty unexpected issue with NSClient++ due to how handles are inherited in Windows.
    Starting a background process in a script will end up blocking the port and forcing a restart of the server. 
    Thus `capture output = false` method is preferred.



# Configuration

<a name="/settings/external scripts"/>
## External script settings

General settings for the external scripts module (CheckExternalScripts).

```ini
# General settings for the external scripts module (CheckExternalScripts).
[/settings/external scripts]
allow arguments=false
allow nasty characters=false
script root=${scripts}
timeout=60

```


| Key                                                                          | Default Value | Description                                                 |
|------------------------------------------------------------------------------|---------------|-------------------------------------------------------------|
| [allow arguments](#/settings/external scripts_allow arguments)               | false         | Allow arguments when executing external scripts             |
| [allow nasty characters](#/settings/external scripts_allow nasty characters) | false         | Allow certain potentially dangerous characters in arguments |
| [script path](#/settings/external scripts_script path)                       |               | Load all scripts in a given folder                          |
| [script root](#/settings/external scripts_script root)                       | ${scripts}    | Script root folder                                          |
| [timeout](#/settings/external scripts_timeout)                               | 60            | Command timeout                                             |




<a name="/settings/external scripts_allow arguments"/>
### allow arguments

**Allow arguments when executing external scripts**

This option determines whether or not the we will allow clients to specify arguments to commands that are executed.




| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | allow arguments                                           |
| Default value: | `false`                                                   |
| Used by:       | CheckExternalScripts                                      |


#### Sample

```
[/settings/external scripts]
# Allow arguments when executing external scripts
allow arguments=false
```


<a name="/settings/external scripts_allow nasty characters"/>
### allow nasty characters

**Allow certain potentially dangerous characters in arguments**

This option determines whether or not the we will allow clients to specify nasty (as in \|\`&><'"\\[]{}) characters in arguments.




| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | allow nasty characters                                    |
| Default value: | `false`                                                   |
| Used by:       | CheckExternalScripts                                      |


#### Sample

```
[/settings/external scripts]
# Allow certain potentially dangerous characters in arguments
allow nasty characters=false
```


<a name="/settings/external scripts_script path"/>
### script path

**Load all scripts in a given folder**

Load all scripts in a given directory and use them as commands.





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | script path                                               |
| Default value: | _N/A_                                                     |
| Used by:       | CheckExternalScripts                                      |


#### Sample

```
[/settings/external scripts]
# Load all scripts in a given folder
script path=
```


<a name="/settings/external scripts_script root"/>
### script root

**Script root folder**

Root path where all scripts are contained (You can not upload/download scripts outside this folder).




| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | script root                                               |
| Default value: | `${scripts}`                                              |
| Used by:       | CheckExternalScripts                                      |


#### Sample

```
[/settings/external scripts]
# Script root folder
script root=${scripts}
```


<a name="/settings/external scripts_timeout"/>
### timeout

**Command timeout**

The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.




| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | timeout                                                   |
| Default value: | `60`                                                      |
| Used by:       | CheckExternalScripts                                      |


#### Sample

```
[/settings/external scripts]
# Command timeout
timeout=60
```


<a name="/settings/external scripts/alias"/>
## Command aliases

A list of aliases for already defined commands (with arguments).
An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)

```ini
# A list of aliases for already defined commands (with arguments).
[/settings/external scripts/alias]

```


| Key                                                                              | Default Value | Description           |
|----------------------------------------------------------------------------------|---------------|-----------------------|
| [alias_cpu](#/settings/external scripts/alias_alias_cpu)                         |               | alias_cpu             |
| [alias_cpu_ex](#/settings/external scripts/alias_alias_cpu_ex)                   |               | alias_cpu_ex          |
| [alias_disk](#/settings/external scripts/alias_alias_disk)                       |               | alias_disk            |
| [alias_disk_loose](#/settings/external scripts/alias_alias_disk_loose)           |               | alias_disk_loose      |
| [alias_event_log](#/settings/external scripts/alias_alias_event_log)             |               | alias_event_log       |
| [alias_file_age](#/settings/external scripts/alias_alias_file_age)               |               | alias_file_age        |
| [alias_file_size](#/settings/external scripts/alias_alias_file_size)             |               | alias_file_size       |
| [alias_mem](#/settings/external scripts/alias_alias_mem)                         |               | alias_mem             |
| [alias_process](#/settings/external scripts/alias_alias_process)                 |               | alias_process         |
| [alias_process_count](#/settings/external scripts/alias_alias_process_count)     |               | alias_process_count   |
| [alias_process_hung](#/settings/external scripts/alias_alias_process_hung)       |               | alias_process_hung    |
| [alias_process_stopped](#/settings/external scripts/alias_alias_process_stopped) |               | alias_process_stopped |
| [alias_sched_all](#/settings/external scripts/alias_alias_sched_all)             |               | alias_sched_all       |
| [alias_sched_long](#/settings/external scripts/alias_alias_sched_long)           |               | alias_sched_long      |
| [alias_sched_task](#/settings/external scripts/alias_alias_sched_task)           |               | alias_sched_task      |
| [alias_service](#/settings/external scripts/alias_alias_service)                 |               | alias_service         |
| [alias_service_ex](#/settings/external scripts/alias_alias_service_ex)           |               | alias_service_ex      |
| [alias_up](#/settings/external scripts/alias_alias_up)                           |               | alias_up              |
| [alias_volumes](#/settings/external scripts/alias_alias_volumes)                 |               | alias_volumes         |
| [alias_volumes_loose](#/settings/external scripts/alias_alias_volumes_loose)     |               | alias_volumes_loose   |




<a name="/settings/external scripts/alias_alias_cpu"/>
### alias_cpu

**alias_cpu**

To configure this create a section under: /settings/external scripts/alias/alias_cpu





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_cpu                                                             |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_cpu
alias_cpu=
```


<a name="/settings/external scripts/alias_alias_cpu_ex"/>
### alias_cpu_ex

**alias_cpu_ex**

To configure this create a section under: /settings/external scripts/alias/alias_cpu_ex





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_cpu_ex                                                          |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_cpu_ex
alias_cpu_ex=
```


<a name="/settings/external scripts/alias_alias_disk"/>
### alias_disk

**alias_disk**

To configure this create a section under: /settings/external scripts/alias/alias_disk





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_disk                                                            |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_disk
alias_disk=
```


<a name="/settings/external scripts/alias_alias_disk_loose"/>
### alias_disk_loose

**alias_disk_loose**

To configure this create a section under: /settings/external scripts/alias/alias_disk_loose





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_disk_loose                                                      |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_disk_loose
alias_disk_loose=
```


<a name="/settings/external scripts/alias_alias_event_log"/>
### alias_event_log

**alias_event_log**

To configure this create a section under: /settings/external scripts/alias/alias_event_log





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_event_log                                                       |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_event_log
alias_event_log=
```


<a name="/settings/external scripts/alias_alias_file_age"/>
### alias_file_age

**alias_file_age**

To configure this create a section under: /settings/external scripts/alias/alias_file_age





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_file_age                                                        |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_file_age
alias_file_age=
```


<a name="/settings/external scripts/alias_alias_file_size"/>
### alias_file_size

**alias_file_size**

To configure this create a section under: /settings/external scripts/alias/alias_file_size





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_file_size                                                       |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_file_size
alias_file_size=
```


<a name="/settings/external scripts/alias_alias_mem"/>
### alias_mem

**alias_mem**

To configure this create a section under: /settings/external scripts/alias/alias_mem





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_mem                                                             |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_mem
alias_mem=
```


<a name="/settings/external scripts/alias_alias_process"/>
### alias_process

**alias_process**

To configure this create a section under: /settings/external scripts/alias/alias_process





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_process                                                         |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_process
alias_process=
```


<a name="/settings/external scripts/alias_alias_process_count"/>
### alias_process_count

**alias_process_count**

To configure this create a section under: /settings/external scripts/alias/alias_process_count





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_process_count                                                   |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_process_count
alias_process_count=
```


<a name="/settings/external scripts/alias_alias_process_hung"/>
### alias_process_hung

**alias_process_hung**

To configure this create a section under: /settings/external scripts/alias/alias_process_hung





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_process_hung                                                    |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_process_hung
alias_process_hung=
```


<a name="/settings/external scripts/alias_alias_process_stopped"/>
### alias_process_stopped

**alias_process_stopped**

To configure this create a section under: /settings/external scripts/alias/alias_process_stopped





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_process_stopped                                                 |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_process_stopped
alias_process_stopped=
```


<a name="/settings/external scripts/alias_alias_sched_all"/>
### alias_sched_all

**alias_sched_all**

To configure this create a section under: /settings/external scripts/alias/alias_sched_all





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_sched_all                                                       |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_sched_all
alias_sched_all=
```


<a name="/settings/external scripts/alias_alias_sched_long"/>
### alias_sched_long

**alias_sched_long**

To configure this create a section under: /settings/external scripts/alias/alias_sched_long





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_sched_long                                                      |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_sched_long
alias_sched_long=
```


<a name="/settings/external scripts/alias_alias_sched_task"/>
### alias_sched_task

**alias_sched_task**

To configure this create a section under: /settings/external scripts/alias/alias_sched_task





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_sched_task                                                      |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_sched_task
alias_sched_task=
```


<a name="/settings/external scripts/alias_alias_service"/>
### alias_service

**alias_service**

To configure this create a section under: /settings/external scripts/alias/alias_service





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_service                                                         |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_service
alias_service=
```


<a name="/settings/external scripts/alias_alias_service_ex"/>
### alias_service_ex

**alias_service_ex**

To configure this create a section under: /settings/external scripts/alias/alias_service_ex





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_service_ex                                                      |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_service_ex
alias_service_ex=
```


<a name="/settings/external scripts/alias_alias_up"/>
### alias_up

**alias_up**

To configure this create a section under: /settings/external scripts/alias/alias_up





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_up                                                              |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_up
alias_up=
```


<a name="/settings/external scripts/alias_alias_volumes"/>
### alias_volumes

**alias_volumes**

To configure this create a section under: /settings/external scripts/alias/alias_volumes





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_volumes                                                         |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_volumes
alias_volumes=
```


<a name="/settings/external scripts/alias_alias_volumes_loose"/>
### alias_volumes_loose

**alias_volumes_loose**

To configure this create a section under: /settings/external scripts/alias/alias_volumes_loose





| Key            | Description                                                           |
|----------------|-----------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias](#/settings/external scripts/alias) |
| Key:           | alias_volumes_loose                                                   |
| Default value: | _N/A_                                                                 |
| Used by:       | CheckExternalScripts                                                  |


#### Sample

```
[/settings/external scripts/alias]
# alias_volumes_loose
alias_volumes_loose=
```


<a name="/settings/external scripts/alias/default"/>
## alias: default

The configuration section for the default alias

```ini
# The configuration section for the default alias
[/settings/external scripts/alias/default]
is template=false
parent=default

```


| Key                                                                  | Default Value | Description |
|----------------------------------------------------------------------|---------------|-------------|
| [alias](#/settings/external scripts/alias/default_alias)             |               | ALIAS       |
| [command](#/settings/external scripts/alias/default_command)         |               | COMMAND     |
| [is template](#/settings/external scripts/alias/default_is template) | false         | IS TEMPLATE |
| [parent](#/settings/external scripts/alias/default_parent)           | default       | PARENT      |




<a name="/settings/external scripts/alias/default_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/default](#/settings/external scripts/alias/default) |
| Key:           | alias                                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Used by:       | CheckExternalScripts                                                                  |


#### Sample

```
[/settings/external scripts/alias/default]
# ALIAS
alias=
```


<a name="/settings/external scripts/alias/default_command"/>
### command

**COMMAND**

Command to execute





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/default](#/settings/external scripts/alias/default) |
| Key:           | command                                                                               |
| Default value: | _N/A_                                                                                 |
| Used by:       | CheckExternalScripts                                                                  |


#### Sample

```
[/settings/external scripts/alias/default]
# COMMAND
command=
```


<a name="/settings/external scripts/alias/default_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/default](#/settings/external scripts/alias/default) |
| Key:           | is template                                                                           |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | `false`                                                                               |
| Used by:       | CheckExternalScripts                                                                  |


#### Sample

```
[/settings/external scripts/alias/default]
# IS TEMPLATE
is template=false
```


<a name="/settings/external scripts/alias/default_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/default](#/settings/external scripts/alias/default) |
| Key:           | parent                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | `default`                                                                             |
| Used by:       | CheckExternalScripts                                                                  |


#### Sample

```
[/settings/external scripts/alias/default]
# PARENT
parent=default
```


<a name="/settings/external scripts/alias/sample"/>
## alias: sample

The configuration section for the sample alias

```ini
# The configuration section for the sample alias
[/settings/external scripts/alias/sample]
is template=false
parent=default

```


| Key                                                                 | Default Value | Description |
|---------------------------------------------------------------------|---------------|-------------|
| [alias](#/settings/external scripts/alias/sample_alias)             |               | ALIAS       |
| [command](#/settings/external scripts/alias/sample_command)         |               | COMMAND     |
| [is template](#/settings/external scripts/alias/sample_is template) | false         | IS TEMPLATE |
| [parent](#/settings/external scripts/alias/sample_parent)           | default       | PARENT      |




<a name="/settings/external scripts/alias/sample_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample) |
| Key:           | alias                                                                               |
| Advanced:      | Yes (means it is not commonly used)                                                 |
| Default value: | _N/A_                                                                               |
| Used by:       | CheckExternalScripts                                                                |


#### Sample

```
[/settings/external scripts/alias/sample]
# ALIAS
alias=
```


<a name="/settings/external scripts/alias/sample_command"/>
### command

**COMMAND**

Command to execute





| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample) |
| Key:           | command                                                                             |
| Default value: | _N/A_                                                                               |
| Sample key:    | Yes (This section is only to show how this key is used)                             |
| Used by:       | CheckExternalScripts                                                                |


#### Sample

```
[/settings/external scripts/alias/sample]
# COMMAND
command=
```


<a name="/settings/external scripts/alias/sample_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample) |
| Key:           | is template                                                                         |
| Advanced:      | Yes (means it is not commonly used)                                                 |
| Default value: | `false`                                                                             |
| Used by:       | CheckExternalScripts                                                                |


#### Sample

```
[/settings/external scripts/alias/sample]
# IS TEMPLATE
is template=false
```


<a name="/settings/external scripts/alias/sample_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/alias/sample](#/settings/external scripts/alias/sample) |
| Key:           | parent                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                                 |
| Default value: | `default`                                                                           |
| Used by:       | CheckExternalScripts                                                                |


#### Sample

```
[/settings/external scripts/alias/sample]
# PARENT
parent=default
```


<a name="/settings/external scripts/scripts"/>
## External scripts

A list of scripts available to run from the CheckExternalScripts module. Syntax is: `command=script arguments`

```ini
# A list of scripts available to run from the CheckExternalScripts module. Syntax is: `command=script arguments`
[/settings/external scripts/scripts]

```






<a name="/settings/external scripts/scripts/default"/>
## script: default

The configuration section for the  default script.

```ini
# The configuration section for the  default script.
[/settings/external scripts/scripts/default]
is template=false
parent=default

```


| Key                                                                            | Default Value | Description      |
|--------------------------------------------------------------------------------|---------------|------------------|
| [alias](#/settings/external scripts/scripts/default_alias)                     |               | ALIAS            |
| [capture output](#/settings/external scripts/scripts/default_capture output)   |               | CAPTURE OUTPUT   |
| [command](#/settings/external scripts/scripts/default_command)                 |               | COMMAND          |
| [display](#/settings/external scripts/scripts/default_display)                 |               | DISPLAY          |
| [domain](#/settings/external scripts/scripts/default_domain)                   |               | DOMAIN           |
| [encoding](#/settings/external scripts/scripts/default_encoding)               |               | ENCODING         |
| [ignore perfdata](#/settings/external scripts/scripts/default_ignore perfdata) |               | IGNORE PERF DATA |
| [is template](#/settings/external scripts/scripts/default_is template)         | false         | IS TEMPLATE      |
| [parent](#/settings/external scripts/scripts/default_parent)                   | default       | PARENT           |
| [password](#/settings/external scripts/scripts/default_password)               |               | PASSWORD         |
| [session](#/settings/external scripts/scripts/default_session)                 |               | SESSION          |
| [user](#/settings/external scripts/scripts/default_user)                       |               | USER             |




<a name="/settings/external scripts/scripts/default_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | alias                                                                                     |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# ALIAS
alias=
```


<a name="/settings/external scripts/scripts/default_capture output"/>
### capture output

**CAPTURE OUTPUT**

This should be set to false if you want to run commands which never terminates (i.e. relinquish control from NSClient++). The effect of this is that the command output will not be captured. The main use is to protect from socket reuse issues





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | capture output                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# CAPTURE OUTPUT
capture output=
```


<a name="/settings/external scripts/scripts/default_command"/>
### command

**COMMAND**

Command to execute





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | command                                                                                   |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# COMMAND
command=
```


<a name="/settings/external scripts/scripts/default_display"/>
### display

**DISPLAY**

Set to true if you want to display the resulting window or not





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | display                                                                                   |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# DISPLAY
display=
```


<a name="/settings/external scripts/scripts/default_domain"/>
### domain

**DOMAIN**

The user to run the command as





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | domain                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# DOMAIN
domain=
```


<a name="/settings/external scripts/scripts/default_encoding"/>
### encoding

**ENCODING**

The encoding to parse the command as





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | encoding                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# ENCODING
encoding=
```


<a name="/settings/external scripts/scripts/default_ignore perfdata"/>
### ignore perfdata

**IGNORE PERF DATA**

Do not parse performance data from the output





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | ignore perfdata                                                                           |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# IGNORE PERF DATA
ignore perfdata=
```


<a name="/settings/external scripts/scripts/default_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | is template                                                                               |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | `false`                                                                                   |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# IS TEMPLATE
is template=false
```


<a name="/settings/external scripts/scripts/default_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | parent                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | `default`                                                                                 |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# PARENT
parent=default
```


<a name="/settings/external scripts/scripts/default_password"/>
### password

**PASSWORD**

The user to run the command as





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | password                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# PASSWORD
password=
```


<a name="/settings/external scripts/scripts/default_session"/>
### session

**SESSION**

Session you want to invoke the client in either the number of current for the one with a UI





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | session                                                                                   |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# SESSION
session=
```


<a name="/settings/external scripts/scripts/default_user"/>
### user

**USER**

The user to run the command as





| Key            | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/default](#/settings/external scripts/scripts/default) |
| Key:           | user                                                                                      |
| Advanced:      | Yes (means it is not commonly used)                                                       |
| Default value: | _N/A_                                                                                     |
| Used by:       | CheckExternalScripts                                                                      |


#### Sample

```
[/settings/external scripts/scripts/default]
# USER
user=
```


<a name="/settings/external scripts/scripts/sample"/>
## script: sample

The configuration section for the  sample script.

```ini
# The configuration section for the  sample script.
[/settings/external scripts/scripts/sample]
is template=false
parent=default

```


| Key                                                                           | Default Value | Description      |
|-------------------------------------------------------------------------------|---------------|------------------|
| [alias](#/settings/external scripts/scripts/sample_alias)                     |               | ALIAS            |
| [capture output](#/settings/external scripts/scripts/sample_capture output)   |               | CAPTURE OUTPUT   |
| [command](#/settings/external scripts/scripts/sample_command)                 |               | COMMAND          |
| [display](#/settings/external scripts/scripts/sample_display)                 |               | DISPLAY          |
| [domain](#/settings/external scripts/scripts/sample_domain)                   |               | DOMAIN           |
| [encoding](#/settings/external scripts/scripts/sample_encoding)               |               | ENCODING         |
| [ignore perfdata](#/settings/external scripts/scripts/sample_ignore perfdata) |               | IGNORE PERF DATA |
| [is template](#/settings/external scripts/scripts/sample_is template)         | false         | IS TEMPLATE      |
| [parent](#/settings/external scripts/scripts/sample_parent)                   | default       | PARENT           |
| [password](#/settings/external scripts/scripts/sample_password)               |               | PASSWORD         |
| [session](#/settings/external scripts/scripts/sample_session)                 |               | SESSION          |
| [user](#/settings/external scripts/scripts/sample_user)                       |               | USER             |




<a name="/settings/external scripts/scripts/sample_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | alias                                                                                   |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# ALIAS
alias=
```


<a name="/settings/external scripts/scripts/sample_capture output"/>
### capture output

**CAPTURE OUTPUT**

This should be set to false if you want to run commands which never terminates (i.e. relinquish control from NSClient++). The effect of this is that the command output will not be captured. The main use is to protect from socket reuse issues





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | capture output                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# CAPTURE OUTPUT
capture output=
```


<a name="/settings/external scripts/scripts/sample_command"/>
### command

**COMMAND**

Command to execute





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | command                                                                                 |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# COMMAND
command=
```


<a name="/settings/external scripts/scripts/sample_display"/>
### display

**DISPLAY**

Set to true if you want to display the resulting window or not





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | display                                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# DISPLAY
display=
```


<a name="/settings/external scripts/scripts/sample_domain"/>
### domain

**DOMAIN**

The user to run the command as





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | domain                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# DOMAIN
domain=
```


<a name="/settings/external scripts/scripts/sample_encoding"/>
### encoding

**ENCODING**

The encoding to parse the command as





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | encoding                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# ENCODING
encoding=
```


<a name="/settings/external scripts/scripts/sample_ignore perfdata"/>
### ignore perfdata

**IGNORE PERF DATA**

Do not parse performance data from the output





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | ignore perfdata                                                                         |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# IGNORE PERF DATA
ignore perfdata=
```


<a name="/settings/external scripts/scripts/sample_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | is template                                                                             |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | `false`                                                                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# IS TEMPLATE
is template=false
```


<a name="/settings/external scripts/scripts/sample_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | parent                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | `default`                                                                               |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# PARENT
parent=default
```


<a name="/settings/external scripts/scripts/sample_password"/>
### password

**PASSWORD**

The user to run the command as





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | password                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# PASSWORD
password=
```


<a name="/settings/external scripts/scripts/sample_session"/>
### session

**SESSION**

Session you want to invoke the client in either the number of current for the one with a UI





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | session                                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# SESSION
session=
```


<a name="/settings/external scripts/scripts/sample_user"/>
### user

**USER**

The user to run the command as





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/scripts/sample](#/settings/external scripts/scripts/sample) |
| Key:           | user                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                                 |
| Used by:       | CheckExternalScripts                                                                    |


#### Sample

```
[/settings/external scripts/scripts/sample]
# USER
user=
```


<a name="/settings/external scripts/wrapped scripts"/>
## Wrapped scripts

A list of wrapped scripts (ie. script using a template mechanism).
The template used will be defined by the extension of the script. Thus a foo.ps1 will use the ps1 wrapping from the wrappings section.

```ini
# A list of wrapped scripts (ie. script using a template mechanism).
[/settings/external scripts/wrapped scripts]

```






<a name="/settings/external scripts/wrappings"/>
## Script wrappings

A list of templates for defining script commands.
Enter any command line here and they will be expanded by scripts placed under the wrapped scripts section. %SCRIPT% will be replaced by the actual script an %ARGS% will be replaced by any given arguments.

```ini
# A list of templates for defining script commands.
[/settings/external scripts/wrappings]
bat=scripts\\%SCRIPT% %ARGS%
ps1=cmd /c echo If (-Not (Test-Path "scripts\%SCRIPT%") ) { Write-Host "UNKNOWN: Script `"%SCRIPT%`" not found."; exit(3) }; scripts\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -
vbs=cscript.exe //T:30 //NoLogo scripts\\lib\\wrapper.vbs %SCRIPT% %ARGS%

```


| Key                                              | Default Value                                                                                                                                                                                                | Description         |
|--------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------|
| [bat](#/settings/external scripts/wrappings_bat) | scripts\\%SCRIPT% %ARGS%                                                                                                                                                                                     | Batch file          |
| [ps1](#/settings/external scripts/wrappings_ps1) | cmd /c echo If (-Not (Test-Path "scripts\%SCRIPT%") ) { Write-Host "UNKNOWN: Script `"%SCRIPT%`" not found."; exit(3) }; scripts\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command - | POWERSHELL WRAPPING |
| [vbs](#/settings/external scripts/wrappings_vbs) | cscript.exe //T:30 //NoLogo scripts\\lib\\wrapper.vbs %SCRIPT% %ARGS%                                                                                                                                        | Visual basic script |




<a name="/settings/external scripts/wrappings_bat"/>
### bat

**Batch file**

Command used for executing wrapped batch files




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/wrappings](#/settings/external scripts/wrappings) |
| Key:           | bat                                                                           |
| Default value: | `scripts\\%SCRIPT% %ARGS%`                                                    |
| Used by:       | CheckExternalScripts                                                          |


#### Sample

```
[/settings/external scripts/wrappings]
# Batch file
bat=scripts\\%SCRIPT% %ARGS%
```


<a name="/settings/external scripts/wrappings_ps1"/>
### ps1

**POWERSHELL WRAPPING**

Command line used for executing wrapped ps1 (powershell) scripts




| Key            | Description                                                                                                                                                                                                    |
|----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/wrappings](#/settings/external scripts/wrappings)                                                                                                                                  |
| Key:           | ps1                                                                                                                                                                                                            |
| Default value: | `cmd /c echo If (-Not (Test-Path "scripts\%SCRIPT%") ) { Write-Host "UNKNOWN: Script `"%SCRIPT%`" not found."; exit(3) }; scripts\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -` |
| Used by:       | CheckExternalScripts                                                                                                                                                                                           |


#### Sample

```
[/settings/external scripts/wrappings]
# POWERSHELL WRAPPING
ps1=cmd /c echo If (-Not (Test-Path "scripts\%SCRIPT%") ) { Write-Host "UNKNOWN: Script `"%SCRIPT%`" not found."; exit(3) }; scripts\%SCRIPT% $ARGS$; exit($lastexitcode) | powershell.exe /noprofile -command -
```


<a name="/settings/external scripts/wrappings_vbs"/>
### vbs

**Visual basic script**

Command line used for wrapped vbs scripts




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/external scripts/wrappings](#/settings/external scripts/wrappings) |
| Key:           | vbs                                                                           |
| Default value: | `cscript.exe //T:30 //NoLogo scripts\\lib\\wrapper.vbs %SCRIPT% %ARGS%`       |
| Used by:       | CheckExternalScripts                                                          |


#### Sample

```
[/settings/external scripts/wrappings]
# Visual basic script
vbs=cscript.exe //T:30 //NoLogo scripts\\lib\\wrapper.vbs %SCRIPT% %ARGS%
```


