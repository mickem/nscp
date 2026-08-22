# Common options and filter keywords

Most check commands are built on top of the same filter engine and therefore accept the same set of
command-line options and expose the same generic filter keywords. They are documented once on this
page; each command's reference page lists which of them the command accepts together with the
command specific default values.

## Common options

These options are available on all filter based commands. Default values are command specific and
listed on each command's reference page.


| Option                                      | Description                                                                                                                                                                           |
|---------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#filter)                           | Filter which marks interesting items.                                                                                                                                                 |
| [warning](#warning)                         | Filter which marks items which generates a warning state.                                                                                                                             |
| [warn](#warn)                               | Short alias for warning                                                                                                                                                               |
| [critical](#critical)                       | Filter which marks items which generates a critical state.                                                                                                                            |
| [crit](#crit)                               | Short alias for critical.                                                                                                                                                             |
| [ok](#ok)                                   | Filter which marks items which generates an ok state.                                                                                                                                 |
| [debug](#debug)                             | Show debugging information in the log                                                                                                                                                 |
| [show-all](#show-all)                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                                                      |
| [empty-state](#empty-state)                 | Return status to use when nothing matched filter.                                                                                                                                     |
| [perf-config](#perf-config)                 | Performance data generation configuration                                                                                                                                             |
| [escape-html](#escape-html)                 | Escape any < and > characters to prevent HTML encoding                                                                                                                                |
| [list-separator](#list-separator)           | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).                                                             |
| [top-syntax](#top-syntax)                   | Top level syntax.                                                                                                                                                                     |
| [ok-syntax](#ok-syntax)                     | ok syntax.                                                                                                                                                                            |
| [empty-syntax](#empty-syntax)               | Empty syntax.                                                                                                                                                                         |
| [detail-syntax](#detail-syntax)             | Detail level syntax.                                                                                                                                                                  |
| [perf-syntax](#perf-syntax)                 | Performance alias syntax.                                                                                                                                                             |
| [unique-index](#unique-index)               | Unique syntax.                                                                                                                                                                        |
| [byte-unit](#byte-unit)                     | Unit to render every byte value of the message in: B, KB, MB, GB, TB, PB or EB.                                                                                                       |
| [decimal-separator](#decimal-separator)     | Character to use as the decimal separator of the message, for instance "," for the European rendering (default ".").                                                                  |
| [decimals](#decimals)                       | Number of decimals to render the numbers of the message with, for instance 1 to turn "25.191GB" into "25.2GB".                                                                        |
| [thousands-separator](#thousands-separator) | Character to group the thousands of the message with, for instance "." to render 1006.85 GB as "1.006,85" (together with decimal-separator=,). Empty (the default) means no grouping. |



<h4 id="filter">filter</h4>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

<h4 id="warning">warning</h4>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


<h4 id="warn">warn</h4>

Short alias for warning

<h4 id="critical">critical</h4>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


<h4 id="crit">crit</h4>

Short alias for critical.

<h4 id="ok">ok</h4>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.

<h4 id="debug">debug</h4>

Show debugging information in the log

<h4 id="show-all">show-all</h4>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<h4 id="empty-state">empty-state</h4>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<h4 id="perf-config">perf-config</h4>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<h4 id="escape-html">escape-html</h4>

Escape any < and > characters to prevent HTML encoding

<h4 id="list-separator">list-separator</h4>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

<h4 id="top-syntax">top-syntax</h4>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

<h4 id="ok-syntax">ok-syntax</h4>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<h4 id="empty-syntax">empty-syntax</h4>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

<h4 id="detail-syntax">detail-syntax</h4>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

<h4 id="perf-syntax">perf-syntax</h4>

Performance alias syntax.
This is the syntax for the base names of the performance data.

<h4 id="unique-index">unique-index</h4>

Unique syntax.
Used to filter unique items (counted will still increase but messages will not repeated)

<h4 id="byte-unit">byte-unit</h4>

Unit to render every byte value of the message in: B, KB, MB, GB, TB, PB or EB.
By default each value scales on its own, which is why a single line can read "140.293GB/0.983TB"; pinning the unit makes the values comparable ("140.29GB/1006.85GB").
Performance data is unaffected - its unit is chosen separately and can be set with perf-config.

<h4 id="decimal-separator">decimal-separator</h4>

Character to use as the decimal separator of the message, for instance "," for the European rendering (default ".").
Only the message is affected: performance data and the numbers you write in a filter or threshold always use ".", as their consumers require.

<h4 id="decimals">decimals</h4>

Number of decimals to render the numbers of the message with, for instance 1 to turn "25.191GB" into "25.2GB".
Applies to the byte and percentage keywords and to format_bytes()/format_number(); -1 (the default) keeps the historical rendering of up to three decimals with the trailing zeros stripped.
Performance data is unaffected - it is generated from the raw values so that graphs keep their full precision.

<h4 id="thousands-separator">thousands-separator</h4>

Character to group the thousands of the message with, for instance "." to render 1006.85 GB as "1.006,85" (together with decimal-separator=,). Empty (the default) means no grouping.
Only the message is affected: performance data and the numbers you write in a filter or threshold are never grouped.


## Standard options

These options are available on every command.


| Option                        | Description                                   |
|-------------------------------|-----------------------------------------------|
| [help](#help)                 | Show help screen (this screen)                |
| [help-pb](#help-pb)           | Show help screen as a protocol buffer payload |
| [show-default](#show-default) | Show default values for a given command       |
| [help-short](#help-short)     | Show help screen (short format).              |



<h4 id="help">help</h4>

Show help screen (this screen)

<h4 id="help-pb">help-pb</h4>

Show help screen as a protocol buffer payload

<h4 id="show-default">show-default</h4>

Show default values for a given command

<h4 id="help-short">help-short</h4>

Show help screen (short format).


## Common filter keywords

These keywords can be used in the filter expressions (`filter`, `warning`, `critical`, `ok`) and in
the syntax templates (`top-syntax`, `detail-syntax`, `perf-syntax`, ...) of every filter based
command, in addition to the command specific keywords listed on each command's reference page.


| Keyword       | Description                                                                                                                                                                                                                                                           |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                                                                                                                                                                                                                  |
| total         | Total number of items.                                                                                                                                                                                                                                                |
| ok_count      | Number of items matched the ok criteria.                                                                                                                                                                                                                              |
| warn_count    | Number of items matched the warning criteria.                                                                                                                                                                                                                         |
| crit_count    | Number of items matched the critical criteria.                                                                                                                                                                                                                        |
| problem_count | Number of items matched either warning or critical criteria.                                                                                                                                                                                                          |
| list          | A list of all items which matched the filter.                                                                                                                                                                                                                         |
| ok_list       | A list of all items which matched the ok criteria.                                                                                                                                                                                                                    |
| warn_list     | A list of all items which matched the warning criteria.                                                                                                                                                                                                               |
| crit_list     | A list of all items which matched the critical criteria.                                                                                                                                                                                                              |
| problem_list  | A list of all items which matched either the critical or the warning criteria.                                                                                                                                                                                        |
| detail_list   | A special list with critical, then warning and finally ok.                                                                                                                                                                                                            |
| sep           | The decoded list-separator, for use in the top-syntax: templates are never escape-decoded (a literal C:\temp must stay a literal C:\temp), so reference %(sep) to break the line before the first list item, e.g. top-syntax=%(status): %(count) items:%(sep)%(list). |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                                                                                                                                                                                                           |


