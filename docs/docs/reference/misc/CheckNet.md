# CheckNet

Network related check such as check_ping.



## List of commands

A list of all available queries (check commands)

| Command                   | Description                             |
|---------------------------|-----------------------------------------|
| [check_ping](#check_ping) | Ping another host and check the result. |







# Queries

A quick reference for all available queries (check commands) in the CheckNet module.

## check_ping

Ping another host and check the result.


### Usage


| Option                                     | Default Value                                     | Description                                                                                                      |
|--------------------------------------------|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_ping_filter)               |                                                   | Filter which marks interesting items.                                                                            |
| [warning](#check_ping_warning)             | time > 60 or loss > 5%                            | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_ping_warn)                   |                                                   | Short alias for warning                                                                                          |
| [critical](#check_ping_critical)           | time > 100 or loss > 10%                          | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_ping_crit)                   |                                                   | Short alias for critical.                                                                                        |
| [ok](#check_ping_ok)                       |                                                   | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_ping_debug)                 | N/A                                               | Show debugging information in the log                                                                            |
| [show-all](#check_ping_show-all)           | N/A                                               | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_ping_empty-state)     | unknown                                           | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_ping_perf-config)     |                                                   | Performance data generation configuration                                                                        |
| [escape-html](#check_ping_escape-html)     | N/A                                               | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_ping_help)                   | N/A                                               | Show help screen (this screen)                                                                                   |
| [help-pb](#check_ping_help-pb)             | N/A                                               | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_ping_show-default)   | N/A                                               | Show default values for a given command                                                                          |
| [help-short](#check_ping_help-short)       | N/A                                               | Show help screen (short format).                                                                                 |
| [top-syntax](#check_ping_top-syntax)       | ${status}: ${ok_count}/${count} (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_ping_ok-syntax)         | %(status): All %(count) hosts are ok              | ok syntax.                                                                                                       |
| [empty-syntax](#check_ping_empty-syntax)   | No hosts found                                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_ping_detail-syntax) | ${ip} Packet loss = ${loss}%, RTA = ${time}ms     | Detail level syntax.                                                                                             |
| [perf-syntax](#check_ping_perf-syntax)     | ${host}                                           | Performance alias syntax.                                                                                        |
| [host](#check_ping_host)                   |                                                   | The host to check (or multiple hosts).                                                                           |
| [total](#check_ping_total)                 | N/A                                               | Include the total of all matching hosts                                                                          |
| [hosts](#check_ping_hosts)                 |                                                   | The host to check (or multiple hosts).                                                                           |
| [count](#check_ping_count)                 | 1                                                 | Number of packets to send.                                                                                       |
| [timeout](#check_ping_timeout)             | 500                                               | Timeout in milliseconds.                                                                                         |


<a name="check_ping_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

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
| host          | The host name or ip address (as given on command line)                                                        |
| ip            | The ip address name                                                                                           |
| loss          | Packet loss                                                                                                   |
| recv          | Number of packets received from the host                                                                      |
| sent          | Number of packets sent to the host                                                                            |
| time          | Round trip time in ms                                                                                         |
| timeout       | Number of packets which timed out from the host                                                               |







<a name="check_ping_warning"/>
### warning


**Deafult Value:** time > 60 or loss > 5%

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

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
| host          | The host name or ip address (as given on command line)                                                        |
| ip            | The ip address name                                                                                           |
| loss          | Packet loss                                                                                                   |
| recv          | Number of packets received from the host                                                                      |
| sent          | Number of packets sent to the host                                                                            |
| time          | Round trip time in ms                                                                                         |
| timeout       | Number of packets which timed out from the host                                                               |







<a name="check_ping_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_ping_critical"/>
### critical


**Deafult Value:** time > 100 or loss > 10%

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

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
| host          | The host name or ip address (as given on command line)                                                        |
| ip            | The ip address name                                                                                           |
| loss          | Packet loss                                                                                                   |
| recv          | Number of packets received from the host                                                                      |
| sent          | Number of packets sent to the host                                                                            |
| time          | Round trip time in ms                                                                                         |
| timeout       | Number of packets which timed out from the host                                                               |







<a name="check_ping_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_ping_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

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
| host          | The host name or ip address (as given on command line)                                                        |
| ip            | The ip address name                                                                                           |
| loss          | Packet loss                                                                                                   |
| recv          | Number of packets received from the host                                                                      |
| sent          | Number of packets sent to the host                                                                            |
| time          | Round trip time in ms                                                                                         |
| timeout       | Number of packets which timed out from the host                                                               |







<a name="check_ping_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_ping_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_ping_empty-state"/>
### empty-state


**Deafult Value:** unknown

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_ping_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_ping_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_ping_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_ping_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_ping_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_ping_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_ping_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${ok_count}/${count} (${problem_list})

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






<a name="check_ping_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): All %(count) hosts are ok

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_ping_empty-syntax"/>
### empty-syntax


**Deafult Value:** No hosts found

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






<a name="check_ping_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${ip} Packet loss = ${loss}%, RTA = ${time}ms

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                                                  |
|---------|--------------------------------------------------------|
| host    | The host name or ip address (as given on command line) |
| ip      | The ip address name                                    |
| loss    | Packet loss                                            |
| recv    | Number of packets received from the host               |
| sent    | Number of packets sent to the host                     |
| time    | Round trip time in ms                                  |
| timeout | Number of packets which timed out from the host        |






<a name="check_ping_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${host}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                                                  |
|---------|--------------------------------------------------------|
| host    | The host name or ip address (as given on command line) |
| ip      | The ip address name                                    |
| loss    | Packet loss                                            |
| recv    | Number of packets received from the host               |
| sent    | Number of packets sent to the host                     |
| time    | Round trip time in ms                                  |
| timeout | Number of packets which timed out from the host        |






<a name="check_ping_host"/>
### host



**Description:**
The host to check (or multiple hosts).

<a name="check_ping_total"/>
### total



**Description:**
Include the total of all matching hosts

<a name="check_ping_hosts"/>
### hosts



**Description:**
The host to check (or multiple hosts).

<a name="check_ping_count"/>
### count


**Deafult Value:** 1

**Description:**
Number of packets to send.

<a name="check_ping_timeout"/>
### timeout


**Deafult Value:** 500

**Description:**
Timeout in milliseconds.



