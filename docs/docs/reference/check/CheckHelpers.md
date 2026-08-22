# CheckHelpers

Various helper function to extend other checks.

## Enable module

To enable this module and and allow using the commands you need to ass `CheckHelpers = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckHelpers = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckHelpers module.

**List of commands:**

A list of all available queries (check commands)

| Command                                         | Description                                                                         |
|-------------------------------------------------|-------------------------------------------------------------------------------------|
| [check_always_critical](#check_always_critical) | Run another check and regardless of its return code return CRITICAL.                |
| [check_always_ok](#check_always_ok)             | Run another check and regardless of its return code return OK.                      |
| [check_always_warning](#check_always_warning)   | Run another check and regardless of its return code return WARNING.                 |
| [check_and_forward](#check_and_forward)         | Run a check and forward the result as a passive check.                              |
| [check_critical](#check_critical)               | Just return CRITICAL (anything passed along will be used as a message).             |
| [check_multi](#check_multi)                     | Run more then one check and return the worst state.                                 |
| [check_negate](#check_negate)                   | Run a check and alter the return status codes according to arguments.               |
| [check_ok](#check_ok)                           | Just return OK (anything passed along will be used as a message).                   |
| [check_timeout](#check_timeout)                 | Run a check and timeout after a given amount of time if the check has not returned. |
| [check_version](#check_version)                 | Just return the NSClient++ version.                                                 |
| [check_warning](#check_warning)                 | Just return WARNING (anything passed along will be used as a message).              |
| [filter_perf](#filter_perf)                     | Run a check and filter performance data.                                            |
| [render_perf](#render_perf)                     | Run a check and render the performance data as output message.                      |
| [xform_perf](#xform_perf)                       | Run a check and transform the performance data in various (currently one) way.      |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command             | Description                               |
|---------------------|-------------------------------------------|
| checkalwayscritical | Alias for: :query:`check_always_critical` |
| checkalwaysok       | Alias for: :query:`check_always_ok`       |
| checkalwayswarning  | Alias for: :query:`check_always_warning`  |
| checkcritical       | Alias for: :query:`check_critical`        |
| checkmultiple       | Alias for: :query:`check_multi`           |
| checkok             | Alias for: :query:`check_ok`              |
| checkversion        | Alias for: :query:`check_version`         |
| checkwarning        | Alias for: :query:`check_warning`         |
| negate              | Alias for: :query:`check_negate`          |
| timeout             | Alias for: :query:`check_timeout`         |

### check_always_critical

Run another check and regardless of its return code return CRITICAL.

**Jump to section:**

* [Command-line Arguments](#check_always_critical_options)



<a id="check_always_critical_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_always_ok

Run another check and regardless of its return code return OK.

**Jump to section:**

* [Command-line Arguments](#check_always_ok_options)



<a id="check_always_ok_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_always_warning

Run another check and regardless of its return code return WARNING.

**Jump to section:**

* [Command-line Arguments](#check_always_warning_options)



<a id="check_always_warning_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_and_forward

Run a check and forward the result as a passive check.

**Jump to section:**

* [Command-line Arguments](#check_and_forward_options)



<a id="check_and_forward_options"></a>
#### Command-line Arguments

<a id="check_and_forward_target"></a>
<a id="check_and_forward_command"></a>
<a id="check_and_forward_arguments"></a>

| Option    | Default Value | Description                                  |
|-----------|---------------|----------------------------------------------|
| target    |               | Commands to run (can be used multiple times) |
| command   |               | Commands to run (can be used multiple times) |
| arguments |               | List of arguments (for wrapped command)      |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_critical

Just return CRITICAL (anything passed along will be used as a message).

**Jump to section:**

* [Command-line Arguments](#check_critical_options)



<a id="check_critical_options"></a>
#### Command-line Arguments

| Option                             | Default Value | Description       |
|------------------------------------|---------------|-------------------|
| [message](#check_critical_message) | No message    | Message to return |



<h5 id="check_critical_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_multi

Run more then one check and return the worst state.

**Jump to section:**

* [Command-line Arguments](#check_multi_options)



<a id="check_multi_options"></a>
#### Command-line Arguments

<a id="check_multi_command"></a>
<a id="check_multi_arguments"></a>
<a id="check_multi_prefix"></a>
<a id="check_multi_suffix"></a>

| Option                              | Default Value | Description                                  |
|-------------------------------------|---------------|----------------------------------------------|
| command                             |               | Commands to run (can be used multiple times) |
| arguments                           |               | Deprecated alias for command                 |
| [separator](#check_multi_separator) | ,             | Separator between messages                   |
| prefix                              |               | Message prefix                               |
| suffix                              |               | Message suffix                               |



<h5 id="check_multi_separator">separator:</h5>

Separator between messages

*Default Value:* `, `


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_negate

Run a check and alter the return status codes according to arguments.

**Jump to section:**

* [Command-line Arguments](#check_negate_options)



<a id="check_negate_options"></a>
#### Command-line Arguments

<a id="check_negate_ok"></a>
<a id="check_negate_warning"></a>
<a id="check_negate_critical"></a>
<a id="check_negate_unknown"></a>
<a id="check_negate_command"></a>
<a id="check_negate_arguments"></a>

| Option    | Default Value | Description                             |
|-----------|---------------|-----------------------------------------|
| ok        |               | The state to return instead of OK       |
| warning   |               | The state to return instead of WARNING  |
| critical  |               | The state to return instead of CRITICAL |
| unknown   |               | The state to return instead of UNKNOWN  |
| command   |               | Wrapped command to execute              |
| arguments |               | List of arguments (for wrapped command) |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_ok

Just return OK (anything passed along will be used as a message).

**Jump to section:**

* [Command-line Arguments](#check_ok_options)



<a id="check_ok_options"></a>
#### Command-line Arguments

| Option                       | Default Value | Description       |
|------------------------------|---------------|-------------------|
| [message](#check_ok_message) | No message    | Message to return |



<h5 id="check_ok_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_timeout

Run a check and timeout after a given amount of time if the check has not returned.

**Jump to section:**

* [Command-line Arguments](#check_timeout_options)



<a id="check_timeout_options"></a>
#### Command-line Arguments

<a id="check_timeout_timeout"></a>
<a id="check_timeout_command"></a>
<a id="check_timeout_arguments"></a>
<a id="check_timeout_return"></a>

| Option    | Default Value | Description                             |
|-----------|---------------|-----------------------------------------|
| timeout   |               | The timeout value                       |
| command   |               | Wrapped command to execute              |
| arguments |               | List of arguments (for wrapped command) |
| return    |               | The return status                       |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_version

Just return the NSClient++ version.

**Jump to section:**

* [Command-line Arguments](#check_version_options)



<a id="check_version_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_warning

Just return WARNING (anything passed along will be used as a message).

**Jump to section:**

* [Command-line Arguments](#check_warning_options)



<a id="check_warning_options"></a>
#### Command-line Arguments

| Option                            | Default Value | Description       |
|-----------------------------------|---------------|-------------------|
| [message](#check_warning_message) | No message    | Message to return |



<h5 id="check_warning_message">message:</h5>

Message to return

*Default Value:* `No message`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### filter_perf

Run a check and filter performance data.

`filter_perf` while badly named can be used to post process performance data.

It can be useful for sorting performance data or limiting the number of performance data items shown.

In its most basic form you can run `filter_perf command=COMMAND arguments REGULAR ARGUMENTS` for example `check_process`:
```
filter_perf command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ws_size'=0GB;3;5 ' ...
```

This will not do anything by itself but we can for instance sort performance data entries by adding `sort=normal`:
```
filter_perf sort=normal command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30851GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 ...
```

And further can also limit the number of results shown by adding `limit=5` like so:
```
filter_perf sort=normal limit=5 command=check_process arguments "filter=exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G"
L        cli WARNING: WARNING: clion64.exe=started
L        cli  Performance data: 'clion64.exe ws_size'=3.30852GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 'msedge.exe ws_size'=0.5757GB;3;5
```

**Jump to section:**

* [Command-line Arguments](#filter_perf_options)



<a id="filter_perf_options"></a>
#### Command-line Arguments

<a id="filter_perf_command"></a>
<a id="filter_perf_arguments"></a>

| Option                      | Default Value | Description                                                 |
|-----------------------------|---------------|-------------------------------------------------------------|
| [sort](#filter_perf_sort)   | none          | The sort order to use: none, normal or reversed             |
| [limit](#filter_perf_limit) | 0             | The maximum number of items to return (0 returns all items) |
| command                     |               | Wrapped command to execute                                  |
| arguments                   |               | List of arguments (for wrapped command)                     |



<h5 id="filter_perf_sort">sort:</h5>

The sort order to use: none, normal or reversed

*Default Value:* `none`

<h5 id="filter_perf_limit">limit:</h5>

The maximum number of items to return (0 returns all items)

*Default Value:* `0`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### render_perf

Run a check and render the performance data as output message.

**Jump to section:**

* [Command-line Arguments](#render_perf_options)
* [Filter keywords](#render_perf_filter_keys)



<a id="render_perf_options"></a>
#### Command-line Arguments

<a id="render_perf_command"></a>
<a id="render_perf_arguments"></a>

| Option                                  | Default Value | Description                             |
|-----------------------------------------|---------------|-----------------------------------------|
| command                                 |               | Wrapped command to execute              |
| arguments                               |               | List of arguments (for wrapped command) |
| [remove-perf](#render_perf_remove-perf) | false         | List of arguments (for wrapped command) |



<h5 id="render_perf_remove-perf">remove-perf:</h5>

List of arguments (for wrapped command)

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                      | Default Value                                          |
|-------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| <a id="render_perf_filter"></a>[filter](../common-options.md#filter)                                        |                                                        |
| <a id="render_perf_warning"></a>[warning](../common-options.md#warning)                                     |                                                        |
| <a id="render_perf_warn"></a>[warn](../common-options.md#warn)                                              |                                                        |
| <a id="render_perf_critical"></a>[critical](../common-options.md#critical)                                  |                                                        |
| <a id="render_perf_crit"></a>[crit](../common-options.md#crit)                                              |                                                        |
| <a id="render_perf_ok"></a>[ok](../common-options.md#ok)                                                    |                                                        |
| <a id="render_perf_debug"></a>[debug](../common-options.md#debug)                                           | false                                                  |
| <a id="render_perf_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                  |
| <a id="render_perf_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                |
| <a id="render_perf_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                        |
| <a id="render_perf_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                  |
| <a id="render_perf_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                      |
| <a id="render_perf_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | %(status): %(message) %(list)                          |
| <a id="render_perf_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                        |
| <a id="render_perf_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                        |
| <a id="render_perf_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | %(key)	%(value)	%(unit)	%(warn)	%(crit)	%(min)	%(max)
 |
| <a id="render_perf_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | %(key)                                                 |
| <a id="render_perf_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                        |
| <a id="render_perf_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                        |
| <a id="render_perf_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                     |
| <a id="render_perf_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="render_perf_filter_keys"></a>
#### Filter keywords

| Option  | Description                                                          |
|---------|----------------------------------------------------------------------|
| crit    | The critical threshold (range when set, otherwise the numeric bound) |
| key     | The name (alias) of the performance data entry                       |
| max     | The maximum bound of the performance data entry                      |
| message | The name (alias) of the performance data entry                       |
| min     | The minimum bound of the performance data entry                      |
| unit    | The unit of the performance data entry                               |
| value   | The value of the performance data entry                              |
| warn    | The warning threshold (range when set, otherwise the numeric bound)  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### xform_perf

Run a check and transform the performance data in various (currently one) way.

**Jump to section:**

* [Command-line Arguments](#xform_perf_options)



<a id="xform_perf_options"></a>
#### Command-line Arguments

<a id="xform_perf_command"></a>
<a id="xform_perf_arguments"></a>
<a id="xform_perf_mode"></a>
<a id="xform_perf_field"></a>
<a id="xform_perf_replace"></a>

| Option    | Default Value | Description                                                                 |
|-----------|---------------|-----------------------------------------------------------------------------|
| command   |               | Wrapped command to execute                                                  |
| arguments |               | List of arguments (for wrapped command)                                     |
| mode      |               | Transformation mode: extract to fetch data or minmax to add missing min/max |
| field     |               | Field to work with (value, warn, crit, max, min)                            |
| replace   |               | Replace expression for the alias                                            |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                    | Description     |
|---------------------------------------------------|-----------------|
| [/settings/check helpers/alias](#command-aliases) | Command aliases |


### Command aliases <a id="/settings/check helpers/alias"></a>

A list of aliases for already-defined commands (with arguments).
An alias is an internal command that has been predefined to provide a single command without arguments. Be careful so you don't create loops (e.g. check_loop=check_a, check_a=check_loop).
Aliases are also available in CheckExternalScripts under [/settings/external scripts/alias]; use this section when you want aliases without enabling external-script execution. If the same alias name is registered by both modules, the last one to load wins - avoid duplicating definitions.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





