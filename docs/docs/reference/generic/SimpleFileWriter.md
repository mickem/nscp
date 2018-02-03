# SimpleFileWriter

Write status updates to a text file (A bit like the NSCA server does)






## Configuration



| Path / Section                         | Description |
|----------------------------------------|-------------|
| [/settings/writers/file](#file-writer) | FILE WRITER |



### FILE WRITER <a id="/settings/writers/file"/>

Section for simple file writer module (SimpleFileWriter.dll).




| Key                                       | Default Value                            | Description            |
|-------------------------------------------|------------------------------------------|------------------------|
| [channel](#channel)                       | FILE                                     | CHANNEL                |
| [file](#file-to-write-to)                 | output.txt                               | FILE TO WRITE TO       |
| [host-syntax](#host-message-syntax)       |                                          | HOST MESSAGE SYNTAX    |
| [service-syntax](#service-message-syntax) |                                          | SERVICE MESSAGE SYNTAX |
| [syntax](#message-syntax)                 | ${alias-or-command} ${result} ${message} | MESSAGE SYNTAX         |
| [time-syntax](#time-syntax)               | %Y-%m-%d %H:%M:%S                        | TIME SYNTAX            |



```ini
# Section for simple file writer module (SimpleFileWriter.dll).
[/settings/writers/file]
channel=FILE
file=output.txt
syntax=${alias-or-command} ${result} ${message}
time-syntax=%Y-%m-%d %H:%M:%S

```





#### CHANNEL <a id="/settings/writers/file/channel"></a>

The channel to listen to.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | channel                                           |
| Default value: | `FILE`                                            |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# CHANNEL
channel=FILE
```



#### FILE TO WRITE TO <a id="/settings/writers/file/file"></a>

The filename to write output to.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | file                                              |
| Default value: | `output.txt`                                      |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# FILE TO WRITE TO
file=output.txt
```



#### HOST MESSAGE SYNTAX <a id="/settings/writers/file/host-syntax"></a>

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.






| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | host-syntax                                       |
| Default value: | _N/A_                                             |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# HOST MESSAGE SYNTAX
host-syntax=
```



#### SERVICE MESSAGE SYNTAX <a id="/settings/writers/file/service-syntax"></a>

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.






| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | service-syntax                                    |
| Default value: | _N/A_                                             |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# SERVICE MESSAGE SYNTAX
service-syntax=
```



#### MESSAGE SYNTAX <a id="/settings/writers/file/syntax"></a>

The syntax of the message to write to the line.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} or ${result_number} = The result status (number), ${epoch} = seconds since unix epoch, ${time} = time using time-format.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | syntax                                            |
| Default value: | `${alias-or-command} ${result} ${message}`        |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# MESSAGE SYNTAX
syntax=${alias-or-command} ${result} ${message}
```



#### TIME SYNTAX <a id="/settings/writers/file/time-syntax"></a>

The date format using strftime format flags. This is the time of writing the message as messages currently does not have a source time.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/writers/file](#/settings/writers/file) |
| Key:           | time-syntax                                       |
| Default value: | `%Y-%m-%d %H:%M:%S`                               |
| Used by:       | SimpleFileWriter                                  |


**Sample:**

```
[/settings/writers/file]
# TIME SYNTAX
time-syntax=%Y-%m-%d %H:%M:%S
```


