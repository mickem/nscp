# CheckLogFile

File for checking log files and various other forms of updating text files



**List of commands:**

A list of all available queries (check commands)

| Command                         | Description                                                             |
|---------------------------------|-------------------------------------------------------------------------|
| [check_logfile](#check_logfile) | Check for errors in log file or generic pattern matching in text files. |


**List of command aliases:**

A list of all short hand aliases for queries (check commands)


| Command      | Description                       |
|--------------|-----------------------------------|
| checklogfile | Alias for: :query:`check_logfile` |


**Configuration Keys:**



    
    
| Path / Section                                              | Key                                             | Description |
|-------------------------------------------------------------|-------------------------------------------------|-------------|
| [/settings/logfile/real-time](#/settings/logfile/real-time) | [enabled](#/settings/logfile/real-time_enabled) | Real time   |


| Path / Section                                                            | Description       |
|---------------------------------------------------------------------------|-------------------|
| [/settings/logfile/real-time/checks](#/settings/logfile/real-time/checks) | Real-time filters |



## Queries

A quick reference for all available queries (check commands) in the CheckLogFile module.

### check_logfile

Check for errors in log file or generic pattern matching in text files.


* [Command-line Arguments](#check_logfile_options)
* [Filter keywords](#check_logfile_filter_keys)





<a name="check_logfile_warn"/>

<a name="check_logfile_crit"/>

<a name="check_logfile_debug"/>

<a name="check_logfile_show-all"/>

<a name="check_logfile_escape-html"/>

<a name="check_logfile_help"/>

<a name="check_logfile_help-pb"/>

<a name="check_logfile_show-default"/>

<a name="check_logfile_help-short"/>

<a name="check_logfile_split"/>

<a name="check_logfile_files"/>

<a name="check_logfile_options"/>
#### Command-line Arguments


| Option                                        | Default Value                       | Description                                                                                                      |
|-----------------------------------------------|-------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_logfile_filter)               |                                     | Filter which marks interesting items.                                                                            |
| [warning](#check_logfile_warning)             |                                     | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                     | Short alias for warning                                                                                          |
| [critical](#check_logfile_critical)           |                                     | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                     | Short alias for critical.                                                                                        |
| [ok](#check_logfile_ok)                       |                                     | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                 | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_logfile_empty-state)     | ignored                             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_logfile_perf-config)     |                                     | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                 | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                 | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                 | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                 | Show default values for a given command                                                                          |
| help-short                                    | N/A                                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_logfile_top-syntax)       | ${count}/${total} (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_logfile_ok-syntax)         |                                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_logfile_empty-syntax)   | %(status): Nothing found            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_logfile_detail-syntax) | ${column1}                          | Detail level syntax.                                                                                             |
| [perf-syntax](#check_logfile_perf-syntax)     | ${column1}                          | Performance alias syntax.                                                                                        |
| [line-split](#check_logfile_line-split)       | \n                                  | Character string used to split a file into several lines (default \n)                                            |
| [column-split](#check_logfile_column-split)   | \t                                  | Character string to split a line into several columns (default \t)                                               |
| split                                         |                                     | Alias for split-column                                                                                           |
| [file](#check_logfile_file)                   |                                     | File to read (can be specified multiple times to check multiple files.                                           |
| files                                         |                                     | A comma separated list of files to scan (same as file except a list)                                             |



<a name="check_logfile_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_logfile_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.






<a name="check_logfile_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.






<a name="check_logfile_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_logfile_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Default Value:* | `ignored`



<a name="check_logfile_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_logfile_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Default Value:* | `${count}/${total} (${problem_list})`



<a name="check_logfile_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).





<a name="check_logfile_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


*Default Value:* | `%(status): Nothing found`



<a name="check_logfile_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Default Value:* | `${column1}`



<a name="check_logfile_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Default Value:* | `${column1}`



<a name="check_logfile_line-split"/>
**line-split:**

Character string used to split a file into several lines (default \n)


*Default Value:* | `\n`



<a name="check_logfile_column-split"/>
**column-split:**

Character string to split a line into several columns (default \t)


*Default Value:* | `\t`



<a name="check_logfile_file"/>
**file:**

File to read (can be specified multiple times to check multiple files.
Notice that specifying multiple files will create an aggregate set it will not check each file individually.
In other words if one file contains an error the entire check will result in error or if you check the count it is the global count which is used.






<a name="check_logfile_filter_keys"/>
#### Filter keywords


| Option                                        | Description                                                                                                  |
|-----------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [column()](#check_logfile_column())           | Fetch the value from the given column number.                                                                |
| [column1](#check_logfile_column1)             | The value in the first column                                                                                |
| [column2](#check_logfile_column2)             | The value in the second column                                                                               |
| [column3](#check_logfile_column3)             | The value in the third column                                                                                |
| [column4](#check_logfile_column4)             | The value in the 4:th column                                                                                 |
| [column5](#check_logfile_column5)             | The value in the 5:th column                                                                                 |
| [column6](#check_logfile_column6)             | The value in the 6:th column                                                                                 |
| [column7](#check_logfile_column7)             | The value in the 7:th column                                                                                 |
| [column8](#check_logfile_column8)             | The value in the 8:th column                                                                                 |
| [column9](#check_logfile_column9)             | The value in the 9:th column                                                                                 |
| [count](#check_logfile_count)                 | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_logfile_crit_count)       | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_logfile_crit_list)         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_logfile_detail_list)     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [file](#check_logfile_file)                   | The name of the file                                                                                         |
| [filename](#check_logfile_filename)           | The name of the file                                                                                         |
| [line](#check_logfile_line)                   | Match the content of an entire line                                                                          |
| [list](#check_logfile_list)                   | A list of all items which matched the filter. Common option for all checks.                                  |
| [ok_count](#check_logfile_ok_count)           | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_logfile_ok_list)             | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [problem_count](#check_logfile_problem_count) | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_logfile_problem_list)   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [status](#check_logfile_status)               | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [total](#check_logfile_total)                 | Total number of items. Common option for all checks.                                                         |
| [warn_count](#check_logfile_warn_count)       | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_logfile_warn_list)         | A list of all items which matched the warning criteria. Common option for all checks.                        |




## Configuration

<a name="/settings/logfile/real-time"/>
### Real-time filtering

A set of options to configure the real time checks




| Key                                             | Default Value | Description |
|-------------------------------------------------|---------------|-------------|
| [enabled](#/settings/logfile/real-time_enabled) | false         | Real time   |



```ini
# A set of options to configure the real time checks
[/settings/logfile/real-time]
enabled=false

```




<a name="/settings/logfile/real-time_enabled"/>

**Real time**

Spawns a background thread which waits for file changes.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/logfile/real-time](#/settings/logfile/real-time) |
| Key:           | enabled                                                     |
| Default value: | `false`                                                     |
| Used by:       | CheckLogFile                                                |


**Sample:**

```
[/settings/logfile/real-time]
# Real time
enabled=false
```


<a name="/settings/logfile/real-time/checks"/>
### Real-time filters

A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key              | Default Value             | Description      |
|------------------|---------------------------|------------------|
| column split     |                           | COLUMN SPLIT     |
| column-split     |                           | COLUMN SPLIT     |
| command          |                           | COMMAND NAME     |
| critical         |                           | CRITICAL FILTER  |
| debug            |                           | DEBUG            |
| destination      |                           | DESTINATION      |
| detail syntax    |                           | SYNTAX           |
| empty message    | eventlog found no records | EMPTY MESSAGE    |
| escape html      |                           | ESCAPE HTML      |
| file             |                           | FILE             |
| files            |                           | FILES            |
| filter           |                           | FILTER           |
| maximum age      | 5m                        | MAGIMUM AGE      |
| ok               |                           | OK FILTER        |
| ok syntax        |                           | SYNTAX           |
| perf config      |                           | PERF CONFIG      |
| read entire file |                           | read entire file |
| severity         |                           | SEVERITY         |
| source id        |                           | SOURCE ID        |
| target           |                           | DESTINATION      |
| target id        |                           | TARGET ID        |
| top syntax       |                           | SYNTAX           |
| warning          |                           | WARNING FILTER   |


**Sample:**

```ini
# An example of a Real-time filters section
[/settings/logfile/real-time/checks/sample]
#column split=...
#column-split=...
#command=...
#critical=...
#debug=...
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#file=...
#files=...
#filter=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#read entire file=...
#severity=...
#source id=...
#target=...
#target id=...
#top syntax=...
#warning=...

```






