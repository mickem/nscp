# SimpleFileWriter

Write status updates to a text file (A bit like the NSCA server does)







## List of Configuration


### Common Keys

| Path / Section                                    | Key                                                      | Description            |
|---------------------------------------------------|----------------------------------------------------------|------------------------|
| [/settings/writers/file](#/settings/writers/file) | [channel](#/settings/writers/file_channel)               | CHANNEL                |
| [/settings/writers/file](#/settings/writers/file) | [file](#/settings/writers/file_file)                     | FILE TO WRITE TO       |
| [/settings/writers/file](#/settings/writers/file) | [host-syntax](#/settings/writers/file_host-syntax)       | HOST MESSAGE SYNTAX    |
| [/settings/writers/file](#/settings/writers/file) | [service-syntax](#/settings/writers/file_service-syntax) | SERVICE MESSAGE SYNTAX |
| [/settings/writers/file](#/settings/writers/file) | [syntax](#/settings/writers/file_syntax)                 | MESSAGE SYNTAX         |
| [/settings/writers/file](#/settings/writers/file) | [time-syntax](#/settings/writers/file_time-syntax)       | TIME SYNTAX            |







# Configuration

<a name="/settings/writers/file"/>
## FILE WRITER

Section for simple file writer module (SimpleFileWriter.dll).

```ini
# Section for simple file writer module (SimpleFileWriter.dll).
[/settings/writers/file]
channel=FILE
file=output.txt
syntax=${alias-or-command} ${result} ${message}
time-syntax=%Y-%m-%d %H:%M:%S

```


| Key                                                      | Default Value                            | Description            |
|----------------------------------------------------------|------------------------------------------|------------------------|
| [channel](#/settings/writers/file_channel)               | FILE                                     | CHANNEL                |
| [file](#/settings/writers/file_file)                     | output.txt                               | FILE TO WRITE TO       |
| [host-syntax](#/settings/writers/file_host-syntax)       |                                          | HOST MESSAGE SYNTAX    |
| [service-syntax](#/settings/writers/file_service-syntax) |                                          | SERVICE MESSAGE SYNTAX |
| [syntax](#/settings/writers/file_syntax)                 | ${alias-or-command} ${result} ${message} | MESSAGE SYNTAX         |
| [time-syntax](#/settings/writers/file_time-syntax)       | %Y-%m-%d %H:%M:%S                        | TIME SYNTAX            |




<a name="/settings/writers/file_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | channel                                           |
| Default value: | `FILE`                                            |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# CHANNEL
channel=FILE
```


<a name="/settings/writers/file_file"/>
### file

**FILE TO WRITE TO**

The filename to write output to.




| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | file                                              |
| Default value: | `output.txt`                                      |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# FILE TO WRITE TO
file=output.txt
```


<a name="/settings/writers/file_host-syntax"/>
### host-syntax

**HOST MESSAGE SYNTAX**

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | host-syntax                                       |
| Default value: | _N/A_                                             |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# HOST MESSAGE SYNTAX
host-syntax=
```


<a name="/settings/writers/file_service-syntax"/>
### service-syntax

**SERVICE MESSAGE SYNTAX**

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | service-syntax                                    |
| Default value: | _N/A_                                             |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# SERVICE MESSAGE SYNTAX
service-syntax=
```


<a name="/settings/writers/file_syntax"/>
### syntax

**MESSAGE SYNTAX**

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.




| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | syntax                                            |
| Default value: | `${alias-or-command} ${result} ${message}`        |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# MESSAGE SYNTAX
syntax=${alias-or-command} ${result} ${message}
```


<a name="/settings/writers/file_time-syntax"/>
### time-syntax

**TIME SYNTAX**

The date format using strftime format flags. This is the time of writing the message as messages currently does not have a source time.




| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | time-syntax                                       |
| Default value: | `%Y-%m-%d %H:%M:%S`                               |
| Used by:       | SimpleFileWriter                                  |


#### Sample

```
[/settings/writers/file]
# TIME SYNTAX
time-syntax=%Y-%m-%d %H:%M:%S
```


