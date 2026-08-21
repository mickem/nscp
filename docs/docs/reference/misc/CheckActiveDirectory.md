# CheckActiveDirectory

*Available on Windows only.*

CheckActiveDirectory checks Active Directory health: replication on domain controllers, the machine-account secure channel and Kerberos KDC availability.

## Enable module

To enable this module and and allow using the commands you need to ass `CheckActiveDirectory = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckActiveDirectory = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckActiveDirectory module.

**List of commands:**

A list of all available queries (check commands)

| Command                                       | Description                                                                                                                 |
|-----------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------|
| [check_ad_replication](#check_ad_replication) | Check inbound Active Directory replication links on a domain controller (last success, consecutive failures). Windows only. |
| [check_kdc](#check_kdc)                       | Check that Kerberos KDCs answer an AS-REQ probe on port 88 (a real Kerberos exchange, not just a port check). Windows only. |
| [check_secure_channel](#check_secure_channel) | Verify the machine-account secure channel to the domain via netlogon. Windows only.                                         |

### check_ad_replication

Check inbound Active Directory replication links on a domain controller (last success, consecutive failures). Windows only.

**Jump to section:**

* [Command-line Arguments](#check_ad_replication_options)
* [Filter keywords](#check_ad_replication_filter_keys)



<a id="check_ad_replication_options"></a>
#### Command-line Arguments

<a id="check_ad_replication_warn"></a>
<a id="check_ad_replication_crit"></a>
<a id="check_ad_replication_help"></a>
<a id="check_ad_replication_help-pb"></a>
<a id="check_ad_replication_show-default"></a>
<a id="check_ad_replication_help-short"></a>
<a id="check_ad_replication_server"></a>

| Option                                                 | Default Value                                                                               | Description                                                                                                               |
|--------------------------------------------------------|---------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_ad_replication_filter)                 |                                                                                             | Filter which marks interesting items.                                                                                     |
| [warning](#check_ad_replication_warning)               | consecutive_failures > 0                                                                    | Filter which marks items which generates a warning state.                                                                 |
| warn                                                   |                                                                                             | Short alias for warning                                                                                                   |
| [critical](#check_ad_replication_critical)             | consecutive_failures > 4 or last_success < -24h                                             | Filter which marks items which generates a critical state.                                                                |
| crit                                                   |                                                                                             | Short alias for critical.                                                                                                 |
| [ok](#check_ad_replication_ok)                         |                                                                                             | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_ad_replication_debug)                   | 1)] (=0                                                                                     | Show debugging information in the log                                                                                     |
| [show-all](#check_ad_replication_show-all)             | 1)] (=0                                                                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_ad_replication_empty-state)       | ok                                                                                          | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_ad_replication_perf-config)       |                                                                                             | Performance data generation configuration                                                                                 |
| [escape-html](#check_ad_replication_escape-html)       | 1)] (=0                                                                                     | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_ad_replication_list-separator) | ,                                                                                           | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                   | N/A                                                                                         | Show help screen (this screen)                                                                                            |
| help-pb                                                | N/A                                                                                         | Show help screen as a protocol buffer payload                                                                             |
| show-default                                           | N/A                                                                                         | Show default values for a given command                                                                                   |
| help-short                                             | N/A                                                                                         | Show help screen (short format).                                                                                          |
| [top-syntax](#check_ad_replication_top-syntax)         | ${status}: ${problem_list}                                                                  | Top level syntax.                                                                                                         |
| [ok-syntax](#check_ad_replication_ok-syntax)           | %(status): all %(count) replication links are healthy                                       | ok syntax.                                                                                                                |
| [empty-syntax](#check_ad_replication_empty-syntax)     | No replication partners found (single domain controller?)                                   | Empty syntax.                                                                                                             |
| [detail-syntax](#check_ad_replication_detail-syntax)   | ${source} ${naming_context}: ${consecutive_failures} failures, last success ${last_success} | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_ad_replication_perf-syntax)       | ${source} ${naming_context}                                                                 | Performance alias syntax.                                                                                                 |
| server                                                 |                                                                                             | The domain controller to check (default: the local machine).                                                              |



<h5 id="check_ad_replication_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_ad_replication_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `consecutive_failures > 0`

<h5 id="check_ad_replication_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `consecutive_failures > 4 or last_success < -24h`

<h5 id="check_ad_replication_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_ad_replication_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_ad_replication_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_ad_replication_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_ad_replication_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_ad_replication_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_ad_replication_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_ad_replication_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_ad_replication_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): all %(count) replication links are healthy`

<h5 id="check_ad_replication_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No replication partners found (single domain controller?)`

<h5 id="check_ad_replication_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${source} ${naming_context}: ${consecutive_failures} failures, last success ${last_success}`

<h5 id="check_ad_replication_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${source} ${naming_context}`


<a id="check_ad_replication_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                     |
|----------------------|-----------------------------------------------------------------|
| consecutive_failures | Number of consecutive failed sync attempts on this link         |
| failed               | True when the last sync attempt failed                          |
| last_attempt         | When the last sync was attempted                                |
| last_error           | Win32 result code of the last sync attempt (0 = success)        |
| last_error_message   | Human readable message for the last sync result (empty when ok) |
| last_success         | When the last sync succeeded (epoch 0 = never)                  |
| naming_context       | The replicated directory partition (naming context) DN          |
| source               | The source domain controller this link replicates from          |
| source_address       | Transport address of the source (GUID-based DNS name)           |
| source_dsa           | Full DN of the source directory service agent                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_kdc

Check that Kerberos KDCs answer an AS-REQ probe on port 88 (a real Kerberos exchange, not just a port check). Windows only.

**Jump to section:**

* [Command-line Arguments](#check_kdc_options)
* [Filter keywords](#check_kdc_filter_keys)



<a id="check_kdc_options"></a>
#### Command-line Arguments

<a id="check_kdc_warn"></a>
<a id="check_kdc_crit"></a>
<a id="check_kdc_help"></a>
<a id="check_kdc_help-pb"></a>
<a id="check_kdc_show-default"></a>
<a id="check_kdc_help-short"></a>
<a id="check_kdc_server"></a>
<a id="check_kdc_realm"></a>

| Option                                      | Default Value                                 | Description                                                                                                               |
|---------------------------------------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_kdc_filter)                 |                                               | Filter which marks interesting items.                                                                                     |
| [warning](#check_kdc_warning)               | time > 1000                                   | Filter which marks items which generates a warning state.                                                                 |
| warn                                        |                                               | Short alias for warning                                                                                                   |
| [critical](#check_kdc_critical)             | responding = 0                                | Filter which marks items which generates a critical state.                                                                |
| crit                                        |                                               | Short alias for critical.                                                                                                 |
| [ok](#check_kdc_ok)                         |                                               | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_kdc_debug)                   | 1)] (=0                                       | Show debugging information in the log                                                                                     |
| [show-all](#check_kdc_show-all)             | 1)] (=0                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_kdc_empty-state)       | ignored                                       | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_kdc_perf-config)       |                                               | Performance data generation configuration                                                                                 |
| [escape-html](#check_kdc_escape-html)       | 1)] (=0                                       | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_kdc_list-separator) | ,                                             | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                        | N/A                                           | Show help screen (this screen)                                                                                            |
| help-pb                                     | N/A                                           | Show help screen as a protocol buffer payload                                                                             |
| show-default                                | N/A                                           | Show default values for a given command                                                                                   |
| help-short                                  | N/A                                           | Show help screen (short format).                                                                                          |
| [top-syntax](#check_kdc_top-syntax)         | ${status}: ${list}                            | Top level syntax.                                                                                                         |
| [ok-syntax](#check_kdc_ok-syntax)           | %(status): all %(count) KDC(s) are responding | ok syntax.                                                                                                                |
| [empty-syntax](#check_kdc_empty-syntax)     |                                               | Empty syntax.                                                                                                             |
| [detail-syntax](#check_kdc_detail-syntax)   | ${kdc}: ${response} (${time}ms)               | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_kdc_perf-syntax)       | ${kdc}                                        | Performance alias syntax.                                                                                                 |
| server                                      |                                               | KDC host to probe; can be given multiple times (default: the KDC located via the domain join).                            |
| realm                                       |                                               | Kerberos realm to request a ticket for (default: the joined domain; required when not domain-joined).                     |
| [port](#check_kdc_port)                     | 88                                            | TCP port to probe.                                                                                                        |
| [timeout](#check_kdc_timeout)               | 5000                                          | Timeout in milliseconds. All KDCs are probed concurrently, so this also bounds the whole check.                           |



<h5 id="check_kdc_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_kdc_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `time > 1000`

<h5 id="check_kdc_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `responding = 0`

<h5 id="check_kdc_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_kdc_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_kdc_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_kdc_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_kdc_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_kdc_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_kdc_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_kdc_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_kdc_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): all %(count) KDC(s) are responding`

<h5 id="check_kdc_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_kdc_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${kdc}: ${response} (${time}ms)`

<h5 id="check_kdc_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${kdc}`

<h5 id="check_kdc_port">port:</h5>

TCP port to probe.

*Default Value:* `88`

<h5 id="check_kdc_timeout">timeout:</h5>

Timeout in milliseconds. All KDCs are probed concurrently, so this also bounds the whole check.

*Default Value:* `5000`


<a id="check_kdc_filter_keys"></a>
#### Filter keywords

| Option     | Description                                                               |
|------------|---------------------------------------------------------------------------|
| error_code | KRB-ERROR code from the response (-1 when none)                           |
| kdc        | The KDC host that was probed                                              |
| port       | TCP port probed                                                           |
| realm      | The Kerberos realm the probe requested a ticket for                       |
| responding | True when the KDC answered the AS-REQ with a well-formed Kerberos message |
| response   | What the KDC answered (or the transport error)                            |
| time       | Probe round-trip time in milliseconds                                     |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_secure_channel

Verify the machine-account secure channel to the domain via netlogon. Windows only.

**Jump to section:**

* [Command-line Arguments](#check_secure_channel_options)
* [Filter keywords](#check_secure_channel_filter_keys)



<a id="check_secure_channel_options"></a>
#### Command-line Arguments

<a id="check_secure_channel_warn"></a>
<a id="check_secure_channel_crit"></a>
<a id="check_secure_channel_help"></a>
<a id="check_secure_channel_help-pb"></a>
<a id="check_secure_channel_show-default"></a>
<a id="check_secure_channel_help-short"></a>
<a id="check_secure_channel_domain"></a>
<a id="check_secure_channel_server"></a>

| Option                                                 | Default Value                                           | Description                                                                                                               |
|--------------------------------------------------------|---------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_secure_channel_filter)                 |                                                         | Filter which marks interesting items.                                                                                     |
| [warning](#check_secure_channel_warning)               |                                                         | Filter which marks items which generates a warning state.                                                                 |
| warn                                                   |                                                         | Short alias for warning                                                                                                   |
| [critical](#check_secure_channel_critical)             | healthy = 0                                             | Filter which marks items which generates a critical state.                                                                |
| crit                                                   |                                                         | Short alias for critical.                                                                                                 |
| [ok](#check_secure_channel_ok)                         |                                                         | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_secure_channel_debug)                   | 1)] (=0                                                 | Show debugging information in the log                                                                                     |
| [show-all](#check_secure_channel_show-all)             | 1)] (=0                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_secure_channel_empty-state)       | ignored                                                 | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_secure_channel_perf-config)       |                                                         | Performance data generation configuration                                                                                 |
| [escape-html](#check_secure_channel_escape-html)       | 1)] (=0                                                 | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_secure_channel_list-separator) | ,                                                       | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                   | N/A                                                     | Show help screen (this screen)                                                                                            |
| help-pb                                                | N/A                                                     | Show help screen as a protocol buffer payload                                                                             |
| show-default                                           | N/A                                                     | Show default values for a given command                                                                                   |
| help-short                                             | N/A                                                     | Show help screen (short format).                                                                                          |
| [top-syntax](#check_secure_channel_top-syntax)         | ${status}: ${list}                                      | Top level syntax.                                                                                                         |
| [ok-syntax](#check_secure_channel_ok-syntax)           |                                                         | ok syntax.                                                                                                                |
| [empty-syntax](#check_secure_channel_empty-syntax)     |                                                         | Empty syntax.                                                                                                             |
| [detail-syntax](#check_secure_channel_detail-syntax)   | secure channel to ${domain} via ${dc}: ${error_message} | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_secure_channel_perf-syntax)       | ${domain}                                               | Performance alias syntax.                                                                                                 |
| domain                                                 |                                                         | The trusted domain to check the channel to (default: the domain this machine is joined to).                               |
| server                                                 |                                                         | The computer whose secure channel to check (default: the local machine).                                                  |
| [verify](#check_secure_channel_verify)                 | 1)] (=1                                                 | Actively verify the channel by contacting the DC (netlogon TC_VERIFY). Set verify=false for a passive status query only.  |



<h5 id="check_secure_channel_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_secure_channel_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_secure_channel_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `healthy = 0`

<h5 id="check_secure_channel_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_secure_channel_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_secure_channel_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_secure_channel_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_secure_channel_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_secure_channel_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_secure_channel_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_secure_channel_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_secure_channel_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_secure_channel_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_secure_channel_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `secure channel to ${domain} via ${dc}: ${error_message}`

<h5 id="check_secure_channel_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${domain}`

<h5 id="check_secure_channel_verify">verify:</h5>

Actively verify the channel by contacting the DC (netlogon TC_VERIFY). Set verify=false for a passive status query only.

*Default Value:* `1)] (=1`


<a id="check_secure_channel_filter_keys"></a>
#### Filter keywords

| Option        | Description                                                  |
|---------------|--------------------------------------------------------------|
| dc            | The domain controller the secure channel is established with |
| domain        | The trusted domain the secure channel points at              |
| error_code    | Win32 status of the secure channel (0 = healthy)             |
| error_message | Human readable channel state (OK or the failure message)     |
| healthy       | True when the secure channel is established and verified     |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

