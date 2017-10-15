# CheckNet

Network related check such as check_ping.



**List of commands:**

A list of all available queries (check commands)

| Command                   | Description                             |
|---------------------------|-----------------------------------------|
| [check_ping](#check_ping) | Ping another host and check the result. |







## Queries

A quick reference for all available queries (check commands) in the CheckNet module.

### check_ping

Ping another host and check the result.


* [Command-line Arguments](#check_ping_options)
* [Filter keywords](#check_ping_filter_keys)





<a name="check_ping_warn"/>

<a name="check_ping_crit"/>

<a name="check_ping_debug"/>

<a name="check_ping_show-all"/>

<a name="check_ping_escape-html"/>

<a name="check_ping_help"/>

<a name="check_ping_help-pb"/>

<a name="check_ping_show-default"/>

<a name="check_ping_help-short"/>

<a name="check_ping_host"/>

<a name="check_ping_total"/>

<a name="check_ping_hosts"/>

<a name="check_ping_options"/>
#### Command-line Arguments


| Option                                     | Default Value                                     | Description                                                                                                      |
|--------------------------------------------|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_ping_filter)               |                                                   | Filter which marks interesting items.                                                                            |
| [warning](#check_ping_warning)             | time > 60 or loss > 5%                            | Filter which marks items which generates a warning state.                                                        |
| warn                                       |                                                   | Short alias for warning                                                                                          |
| [critical](#check_ping_critical)           | time > 100 or loss > 10%                          | Filter which marks items which generates a critical state.                                                       |
| crit                                       |                                                   | Short alias for critical.                                                                                        |
| [ok](#check_ping_ok)                       |                                                   | Filter which marks items which generates an ok state.                                                            |
| debug                                      | N/A                                               | Show debugging information in the log                                                                            |
| show-all                                   | N/A                                               | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_ping_empty-state)     | unknown                                           | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_ping_perf-config)     |                                                   | Performance data generation configuration                                                                        |
| escape-html                                | N/A                                               | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                       | N/A                                               | Show help screen (this screen)                                                                                   |
| help-pb                                    | N/A                                               | Show help screen as a protocol buffer payload                                                                    |
| show-default                               | N/A                                               | Show default values for a given command                                                                          |
| help-short                                 | N/A                                               | Show help screen (short format).                                                                                 |
| [top-syntax](#check_ping_top-syntax)       | ${status}: ${ok_count}/${count} (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_ping_ok-syntax)         | %(status): All %(count) hosts are ok              | ok syntax.                                                                                                       |
| [empty-syntax](#check_ping_empty-syntax)   | No hosts found                                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_ping_detail-syntax) | ${ip} Packet loss = ${loss}%, RTA = ${time}ms     | Detail level syntax.                                                                                             |
| [perf-syntax](#check_ping_perf-syntax)     | ${host}                                           | Performance alias syntax.                                                                                        |
| host                                       |                                                   | The host to check (or multiple hosts).                                                                           |
| total                                      | N/A                                               | Include the total of all matching hosts                                                                          |
| hosts                                      |                                                   | The host to check (or multiple hosts).                                                                           |
| [count](#check_ping_count)                 | 1                                                 | Number of packets to send.                                                                                       |
| [timeout](#check_ping_timeout)             | 500                                               | Timeout in milliseconds.                                                                                         |



<a name="check_ping_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_ping_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



*Deafult Value:* | `time > 60 or loss > 5%`



<a name="check_ping_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



*Deafult Value:* | `time > 100 or loss > 10%`



<a name="check_ping_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_ping_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Deafult Value:* | `unknown`



<a name="check_ping_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_ping_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${status}: ${ok_count}/${count} (${problem_list})`



<a name="check_ping_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


*Deafult Value:* | `%(status): All %(count) hosts are ok`



<a name="check_ping_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


*Deafult Value:* | `No hosts found`



<a name="check_ping_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${ip} Packet loss = ${loss}%, RTA = ${time}ms`



<a name="check_ping_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Deafult Value:* | `${host}`



<a name="check_ping_count"/>
**count:**

Number of packets to send.


*Deafult Value:* | `1`



<a name="check_ping_timeout"/>
**timeout:**

Timeout in milliseconds.


*Deafult Value:* | `500`




<a name="check_ping_filter_keys"/>
#### Filter keywords


| Option                                     | Description                                                                                                  |
|--------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [count](#check_ping_count)                 | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_ping_crit_count)       | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_ping_crit_list)         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_ping_detail_list)     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [host](#check_ping_host)                   | The host name or ip address (as given on command line)                                                       |
| [ip](#check_ping_ip)                       | The ip address name                                                                                          |
| [list](#check_ping_list)                   | A list of all items which matched the filter. Common option for all checks.                                  |
| [loss](#check_ping_loss)                   | Packet loss                                                                                                  |
| [ok_count](#check_ping_ok_count)           | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_ping_ok_list)             | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [problem_count](#check_ping_problem_count) | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_ping_problem_list)   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [recv](#check_ping_recv)                   | Number of packets received from the host                                                                     |
| [sent](#check_ping_sent)                   | Number of packets sent to the host                                                                           |
| [status](#check_ping_status)               | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [time](#check_ping_time)                   | Round trip time in ms                                                                                        |
| [timeout](#check_ping_timeout)             | Number of packets which timed out from the host                                                              |
| [total](#check_ping_total)                 | Total number of items. Common option for all checks.                                                         |
| [warn_count](#check_ping_warn_count)       | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_ping_warn_list)         | A list of all items which matched the warning criteria. Common option for all checks.                        |




