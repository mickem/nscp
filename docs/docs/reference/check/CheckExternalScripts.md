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




**List of command aliases:**

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


**Configuration Keys:**



    
    
    
    
    
| Path / Section                                            | Key                                                                          | Description                                                 |
|-----------------------------------------------------------|------------------------------------------------------------------------------|-------------------------------------------------------------|
| [/settings/external scripts](#/settings/external scripts) | [allow arguments](#/settings/external scripts_allow arguments)               | Allow arguments when executing external scripts             |
| [/settings/external scripts](#/settings/external scripts) | [allow nasty characters](#/settings/external scripts_allow nasty characters) | Allow certain potentially dangerous characters in arguments |
| [/settings/external scripts](#/settings/external scripts) | [script path](#/settings/external scripts_script path)                       | Load all scripts in a given folder                          |
| [/settings/external scripts](#/settings/external scripts) | [script root](#/settings/external scripts_script root)                       | Script root folder                                          |
| [/settings/external scripts](#/settings/external scripts) | [timeout](#/settings/external scripts_timeout)                               | Command timeout                                             |


| Path / Section                                                                            | Description      |
|-------------------------------------------------------------------------------------------|------------------|
| [/settings/external scripts/alias](#/settings/external scripts/alias)                     | Command aliases  |
| [/settings/external scripts/scripts](#/settings/external scripts/scripts)                 | External scripts |
| [/settings/external scripts/wrapped scripts](#/settings/external scripts/wrapped scripts) | Wrapped scripts  |
| [/settings/external scripts/wrappings](#/settings/external scripts/wrappings)             | Script wrappings |



## Samples

_Feel free to add more samples [on this page](https://github.com/mickem/docs/blob/master/docs/samples/CheckExternalScripts_samples.md)_

### Adding a simple script

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

### Using arguments

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

### Running a command as a user

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

### Programs "running forever"

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



## Configuration

<a name="/settings/external scripts"/>
### External script settings

General settings for the external scripts module (CheckExternalScripts).




| Key                                                                          | Default Value | Description                                                 |
|------------------------------------------------------------------------------|---------------|-------------------------------------------------------------|
| [allow arguments](#/settings/external scripts_allow arguments)               | false         | Allow arguments when executing external scripts             |
| [allow nasty characters](#/settings/external scripts_allow nasty characters) | false         | Allow certain potentially dangerous characters in arguments |
| [script path](#/settings/external scripts_script path)                       |               | Load all scripts in a given folder                          |
| [script root](#/settings/external scripts_script root)                       | ${scripts}    | Script root folder                                          |
| [timeout](#/settings/external scripts_timeout)                               | 60            | Command timeout                                             |



```ini
# General settings for the external scripts module (CheckExternalScripts).
[/settings/external scripts]
allow arguments=false
allow nasty characters=false
script root=${scripts}
timeout=60

```




<a name="/settings/external scripts_allow arguments"/>

**Allow arguments when executing external scripts**

This option determines whether or not the we will allow clients to specify arguments to commands that are executed.





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | allow arguments                                           |
| Default value: | `false`                                                   |
| Used by:       | CheckExternalScripts                                      |


**Sample:**

```
[/settings/external scripts]
# Allow arguments when executing external scripts
allow arguments=false
```


<a name="/settings/external scripts_allow nasty characters"/>

**Allow certain potentially dangerous characters in arguments**

This option determines whether or not the we will allow clients to specify nasty (as in \|\`&><'"\\[]{}) characters in arguments.





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | allow nasty characters                                    |
| Default value: | `false`                                                   |
| Used by:       | CheckExternalScripts                                      |


**Sample:**

```
[/settings/external scripts]
# Allow certain potentially dangerous characters in arguments
allow nasty characters=false
```


<a name="/settings/external scripts_script path"/>

**Load all scripts in a given folder**

Load all scripts in a given directory and use them as commands.






| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | script path                                               |
| Default value: | _N/A_                                                     |
| Used by:       | CheckExternalScripts                                      |


**Sample:**

```
[/settings/external scripts]
# Load all scripts in a given folder
script path=
```


<a name="/settings/external scripts_script root"/>

**Script root folder**

Root path where all scripts are contained (You can not upload/download scripts outside this folder).





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | script root                                               |
| Default value: | `${scripts}`                                              |
| Used by:       | CheckExternalScripts                                      |


**Sample:**

```
[/settings/external scripts]
# Script root folder
script root=${scripts}
```


<a name="/settings/external scripts_timeout"/>

**Command timeout**

The maximum time in seconds that a command can execute. (if more then this execution will be aborted). NOTICE this only affects external commands not internal ones.





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/external scripts](#/settings/external scripts) |
| Key:           | timeout                                                   |
| Default value: | `60`                                                      |
| Used by:       | CheckExternalScripts                                      |


**Sample:**

```
[/settings/external scripts]
# Command timeout
timeout=60
```


<a name="/settings/external scripts/alias"/>
### Command aliases

A list of aliases for already defined commands (with arguments).
An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (ie check_loop=check_a, check_a=check_loop)


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key         | Default Value | Description |
|-------------|---------------|-------------|
| alias       |               | ALIAS       |
| command     |               | COMMAND     |
| is template | false         | IS TEMPLATE |
| parent      | default       | PARENT      |


**Sample:**

```ini
# An example of a Command aliases section
[/settings/external scripts/alias/sample]
#alias=...
#command=...
is template=false
parent=default

```



**Known instances:**

*  alias_sched_task
*  alias_sched_long
*  alias_file_size
*  alias_service
*  alias_sched_all
*  alias_disk
*  alias_process_hung
*  alias_up
*  alias_event_log
*  alias_volumes
*  alias_process_count
*  alias_volumes_loose
*  alias_disk_loose
*  alias_process_stopped
*  alias_cpu
*  alias_file_age
*  alias_service_ex
*  alias_process
*  alias_cpu_ex
*  alias_mem







<a name="/settings/external scripts/scripts"/>
### External scripts

A list of scripts available to run from the CheckExternalScripts module. Syntax is: `command=script arguments`


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key             | Default Value | Description      |
|-----------------|---------------|------------------|
| alias           |               | ALIAS            |
| capture output  |               | CAPTURE OUTPUT   |
| command         |               | COMMAND          |
| display         |               | DISPLAY          |
| domain          |               | DOMAIN           |
| encoding        |               | ENCODING         |
| ignore perfdata |               | IGNORE PERF DATA |
| is template     | false         | IS TEMPLATE      |
| parent          | default       | PARENT           |
| password        |               | PASSWORD         |
| session         |               | SESSION          |
| user            |               | USER             |


**Sample:**

```ini
# An example of a External scripts section
[/settings/external scripts/scripts/sample]
#alias=...
#capture output=...
#command=...
#display=...
#domain=...
#encoding=...
#ignore perfdata=...
is template=false
parent=default
#password=...
#session=...
#user=...

```






<a name="/settings/external scripts/wrapped scripts"/>
### Wrapped scripts

A list of wrapped scripts (ie. script using a template mechanism).
The template used will be defined by the extension of the script. Thus a foo.ps1 will use the ps1 wrapping from the wrappings section.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






<a name="/settings/external scripts/wrappings"/>
### Script wrappings

A list of templates for defining script commands.
Enter any command line here and they will be expanded by scripts placed under the wrapped scripts section. %SCRIPT% will be replaced by the actual script an %ARGS% will be replaced by any given arguments.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.



**Known instances:**

*  vbs
*  bat
*  ps1







