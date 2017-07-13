# CheckLogFile

File for checking log files and various other forms of updating text files



## List of commands

A list of all available queries (check commands)

| Command                         | Description                                                             |
|---------------------------------|-------------------------------------------------------------------------|
| [check_logfile](#check_logfile) | Check for errors in log file or generic pattern matching in text files. |


## List of command aliases

A list of all short hand aliases for queries (check commands)


| Command      | Description                       |
|--------------|-----------------------------------|
| checklogfile | Alias for: :query:`check_logfile` |


## List of Configuration


### Common Keys

| Path / Section                                              | Key                                             | Description        |
|-------------------------------------------------------------|-------------------------------------------------|--------------------|
| [/settings/logfile/real-time](#/settings/logfile/real-time) | [enabled](#/settings/logfile/real-time_enabled) | REAL TIME CHECKING |





# Queries

A quick reference for all available queries (check commands) in the CheckLogFile module.

## check_logfile

Check for errors in log file or generic pattern matching in text files.


### Usage


| Option                                        | Default Value                       | Description                                                                                                      |
|-----------------------------------------------|-------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_logfile_filter)               |                                     | Filter which marks interesting items.                                                                            |
| [warning](#check_logfile_warning)             |                                     | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_logfile_warn)                   |                                     | Short alias for warning                                                                                          |
| [critical](#check_logfile_critical)           |                                     | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_logfile_crit)                   |                                     | Short alias for critical.                                                                                        |
| [ok](#check_logfile_ok)                       |                                     | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_logfile_debug)                 | N/A                                 | Show debugging information in the log                                                                            |
| [show-all](#check_logfile_show-all)           | N/A                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_logfile_empty-state)     | ignored                             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_logfile_perf-config)     |                                     | Performance data generation configuration                                                                        |
| [escape-html](#check_logfile_escape-html)     | N/A                                 | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_logfile_help)                   | N/A                                 | Show help screen (this screen)                                                                                   |
| [help-pb](#check_logfile_help-pb)             | N/A                                 | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_logfile_show-default)   | N/A                                 | Show default values for a given command                                                                          |
| [help-short](#check_logfile_help-short)       | N/A                                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_logfile_top-syntax)       | ${count}/${total} (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_logfile_ok-syntax)         |                                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_logfile_empty-syntax)   | %(status): Nothing found            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_logfile_detail-syntax) | ${column1}                          | Detail level syntax.                                                                                             |
| [perf-syntax](#check_logfile_perf-syntax)     | ${column1}                          | Performance alias syntax.                                                                                        |
| [line-split](#check_logfile_line-split)       | \n                                  | Character string used to split a file into several lines (default \n)                                            |
| [column-split](#check_logfile_column-split)   | \t                                  | Character string to split a line into several columns (default \t)                                               |
| [split](#check_logfile_split)                 |                                     | Alias for split-column                                                                                           |
| [file](#check_logfile_file)                   |                                     | File to read (can be specified multiple times to check multiple files.                                           |
| [files](#check_logfile_files)                 |                                     | A comma separated list of files to scan (same as file except a list)                                             |


<a name="check_logfile_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key                             |
|---------------------------------|
| count                           |
| total                           |
| ok_count                        |
| warn_count                      |
| crit_count                      |
| problem_count                   |
| list                            |
| ok_list                         |
| warn_list                       |
| crit_list                       |
| problem_list                    |
| detail_list                     |
| status                          |
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |







<a name="check_logfile_warning"/>
### warning



**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key                             |
|---------------------------------|
| count                           |
| total                           |
| ok_count                        |
| warn_count                      |
| crit_count                      |
| problem_count                   |
| list                            |
| ok_list                         |
| warn_list                       |
| crit_list                       |
| problem_list                    |
| detail_list                     |
| status                          |
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |







<a name="check_logfile_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_logfile_critical"/>
### critical



**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key                             |
|---------------------------------|
| count                           |
| total                           |
| ok_count                        |
| warn_count                      |
| crit_count                      |
| problem_count                   |
| list                            |
| ok_list                         |
| warn_list                       |
| crit_list                       |
| problem_list                    |
| detail_list                     |
| status                          |
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |







<a name="check_logfile_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_logfile_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key                             |
|---------------------------------|
| count                           |
| total                           |
| ok_count                        |
| warn_count                      |
| crit_count                      |
| problem_count                   |
| list                            |
| ok_list                         |
| warn_list                       |
| crit_list                       |
| problem_list                    |
| detail_list                     |
| status                          |
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |







<a name="check_logfile_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_logfile_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_logfile_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_logfile_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_logfile_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_logfile_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_logfile_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_logfile_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_logfile_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_logfile_top-syntax"/>
### top-syntax


**Deafult Value:** ${count}/${total} (${problem_list})

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_logfile_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_logfile_empty-syntax"/>
### empty-syntax


**Deafult Value:** %(status): Nothing found

**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_logfile_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${column1}

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key                             |
|---------------------------------|
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |






<a name="check_logfile_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${column1}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key                             |
|---------------------------------|
| column1                         |
| column2                         |
| column3                         |
| column4                         |
| column5                         |
| column6                         |
| column7                         |
| column8                         |
| column9                         |
| file                            |
| filename                        |
| line                            |
| column()                        |
| Syntax: column(<coulmn number>) |






<a name="check_logfile_line-split"/>
### line-split


**Deafult Value:** \n

**Description:**
Character string used to split a file into several lines (default \n)

<a name="check_logfile_column-split"/>
### column-split


**Deafult Value:** \t

**Description:**
Character string to split a line into several columns (default \t)

<a name="check_logfile_split"/>
### split



**Description:**
Alias for split-column

<a name="check_logfile_file"/>
### file



**Description:**
File to read (can be specified multiple times to check multiple files.
Notice that specifying multiple files will create an aggregate set it will not check each file individually.
In other words if one file contains an error the entire check will result in error or if you check the count it is the global count which is used.

<a name="check_logfile_files"/>
### files



**Description:**
A comma separated list of files to scan (same as file except a list)



# Configuration

<a name="/settings/logfile"/>
## LOG FILE SECTION

Section for log file checker

```ini
# Section for log file checker
[/settings/logfile]

```






<a name="/settings/logfile/real-time"/>
## CONFIGURE REALTIME CHECKING

A set of options to configure the real time checks

```ini
# A set of options to configure the real time checks
[/settings/logfile/real-time]
enabled=false

```


| Key                                             | Default Value | Description        |
|-------------------------------------------------|---------------|--------------------|
| [enabled](#/settings/logfile/real-time_enabled) | false         | REAL TIME CHECKING |




<a name="/settings/logfile/real-time_enabled"/>
### enabled

**REAL TIME CHECKING**

Spawns a background thread which waits for file changes.




| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/logfile/real-time](#/settings/logfile/real-time) |
| Key:           | enabled                                                     |
| Default value: | `false`                                                     |
| Used by:       | CheckLogFile                                                |


#### Sample

```
[/settings/logfile/real-time]
# REAL TIME CHECKING
enabled=false
```


<a name="/settings/logfile/real-time/checks"/>
## REALTIME FILTERS

A set of filters to use in real-time mode

```ini
# A set of filters to use in real-time mode
[/settings/logfile/real-time/checks]

```






