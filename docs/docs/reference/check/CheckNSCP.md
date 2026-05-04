# CheckNSCP

Use this module to check the health and status of NSClient++ it self



## Enable module

To enable this module and and allow using the commands you need to ass `CheckNSCP = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckNSCP = enabled
```


## Queries

A quick reference for all available queries (check commands) in the CheckNSCP module.

**List of commands:**

A list of all available queries (check commands)

| Command                                   | Description                                                                                                                                              |
|-------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| [check_nscp](#check_nscp)                 | Check the internal health of NSClient++.                                                                                                                 |
| [check_nscp_update](#check_nscp_update)   | Check if there is a newer version of NSClient++ available on GitHub. The result is cached (default 24 hours) to avoid hitting the GitHub API rate limit. |
| [check_nscp_version](#check_nscp_version) | Check the version of NSClient++ which is used.                                                                                                           |




### check_nscp

Check the internal health of NSClient++.


**Jump to section:**

* [Command-line Arguments](#check_nscp_options)





<a id="check_nscp_help"></a>
<a id="check_nscp_help-pb"></a>
<a id="check_nscp_show-default"></a>
<a id="check_nscp_help-short"></a>
<a id="check_nscp_options"></a>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |




### check_nscp_update

Check if there is a newer version of NSClient++ available on GitHub. The result is cached (default 24 hours) to avoid hitting the GitHub API rate limit.


**Jump to section:**

* [Command-line Arguments](#check_nscp_update_options)
* [Filter keywords](#check_nscp_update_filter_keys)





<a id="check_nscp_update_warn"></a>
<a id="check_nscp_update_crit"></a>
<a id="check_nscp_update_debug"></a>
<a id="check_nscp_update_show-all"></a>
<a id="check_nscp_update_escape-html"></a>
<a id="check_nscp_update_help"></a>
<a id="check_nscp_update_help-pb"></a>
<a id="check_nscp_update_show-default"></a>
<a id="check_nscp_update_help-short"></a>
<a id="check_nscp_update_options"></a>
#### Command-line Arguments


| Option                                            | Default Value                          | Description                                                                                                      |
|---------------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_nscp_update_filter)               |                                        | Filter which marks interesting items.                                                                            |
| [warning](#check_nscp_update_warning)             | update_available = 1                   | Filter which marks items which generates a warning state.                                                        |
| warn                                              |                                        | Short alias for warning                                                                                          |
| [critical](#check_nscp_update_critical)           | update_available = 1                   | Filter which marks items which generates a critical state.                                                       |
| crit                                              |                                        | Short alias for critical.                                                                                        |
| [ok](#check_nscp_update_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                             | N/A                                    | Show debugging information in the log                                                                            |
| show-all                                          | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_nscp_update_empty-state)     | ignored                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_nscp_update_perf-config)     |                                        | Performance data generation configuration                                                                        |
| escape-html                                       | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                              | N/A                                    | Show help screen (this screen)                                                                                   |
| help-pb                                           | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                                      | N/A                                    | Show default values for a given command                                                                          |
| help-short                                        | N/A                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_nscp_update_top-syntax)       | ${status}: ${list}                     | Top level syntax.                                                                                                |
| [ok-syntax](#check_nscp_update_ok-syntax)         |                                        | ok syntax.                                                                                                       |
| [empty-syntax](#check_nscp_update_empty-syntax)   |                                        | Empty syntax.                                                                                                    |
| [detail-syntax](#check_nscp_update_detail-syntax) | ${version} (latest: ${latest_version}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_nscp_update_perf-syntax)     | version                                | Performance alias syntax.                                                                                        |



<h5 id="check_nscp_update_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_nscp_update_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `update_available = 1`

<h5 id="check_nscp_update_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `update_available = 1`

<h5 id="check_nscp_update_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_nscp_update_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_nscp_update_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_nscp_update_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_nscp_update_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_nscp_update_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_nscp_update_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${version} (latest: ${latest_version})`

<h5 id="check_nscp_update_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `version`


<a id="check_nscp_update_filter_keys"></a>
#### Filter keywords


| Option           | Description                                                                                                                      |
|------------------|----------------------------------------------------------------------------------------------------------------------------------|
| build            | The build component of the installed version (the 3 in 0.1.2.3)                                                                  |
| date             | The build date of the currently installed NSClient++                                                                             |
| error            | Error message if the latest version could not be determined (empty when ok)                                                      |
| latest_build     | The build component of the latest available version                                                                              |
| latest_major     | The major component of the latest available version                                                                              |
| latest_minor     | The minor component of the latest available version                                                                              |
| latest_release   | The release component of the latest available version                                                                            |
| latest_version   | The latest available NSClient++ version (empty if lookup failed)                                                                 |
| major            | The major component of the installed version (the 1 in 0.1.2.3)                                                                  |
| minor            | The minor component of the installed version (the 2 in 0.1.2.3)                                                                  |
| published        | Publication date of the latest release                                                                                           |
| release          | The release component of the installed version (the 0 in 0.1.2.3)                                                                |
| tag              | The GitHub tag of the latest release                                                                                             |
| update_available | 1 when the latest available version is newer than the running version, 0 otherwise (and 0 if the lookup failed)                  |
| url              | URL of the latest release on GitHub                                                                                              |
| version          | The currently installed NSClient++ version                                                                                       |
| versions_behind  | Difference between latest and current version components (largest meaningful component) when an update is available, 0 otherwise |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_nscp_version

Check the version of NSClient++ which is used.


**Jump to section:**

* [Command-line Arguments](#check_nscp_version_options)
* [Filter keywords](#check_nscp_version_filter_keys)





<a id="check_nscp_version_warn"></a>
<a id="check_nscp_version_crit"></a>
<a id="check_nscp_version_debug"></a>
<a id="check_nscp_version_show-all"></a>
<a id="check_nscp_version_escape-html"></a>
<a id="check_nscp_version_help"></a>
<a id="check_nscp_version_help-pb"></a>
<a id="check_nscp_version_show-default"></a>
<a id="check_nscp_version_help-short"></a>
<a id="check_nscp_version_options"></a>
#### Command-line Arguments


| Option                                             | Default Value        | Description                                                                                                      |
|----------------------------------------------------|----------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_nscp_version_filter)               |                      | Filter which marks interesting items.                                                                            |
| [warning](#check_nscp_version_warning)             |                      | Filter which marks items which generates a warning state.                                                        |
| warn                                               |                      | Short alias for warning                                                                                          |
| [critical](#check_nscp_version_critical)           |                      | Filter which marks items which generates a critical state.                                                       |
| crit                                               |                      | Short alias for critical.                                                                                        |
| [ok](#check_nscp_version_ok)                       |                      | Filter which marks items which generates an ok state.                                                            |
| debug                                              | N/A                  | Show debugging information in the log                                                                            |
| show-all                                           | N/A                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_nscp_version_empty-state)     | ignored              | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_nscp_version_perf-config)     |                      | Performance data generation configuration                                                                        |
| escape-html                                        | N/A                  | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                               | N/A                  | Show help screen (this screen)                                                                                   |
| help-pb                                            | N/A                  | Show help screen as a protocol buffer payload                                                                    |
| show-default                                       | N/A                  | Show default values for a given command                                                                          |
| help-short                                         | N/A                  | Show help screen (short format).                                                                                 |
| [top-syntax](#check_nscp_version_top-syntax)       | ${status}: ${list}   | Top level syntax.                                                                                                |
| [ok-syntax](#check_nscp_version_ok-syntax)         |                      | ok syntax.                                                                                                       |
| [empty-syntax](#check_nscp_version_empty-syntax)   |                      | Empty syntax.                                                                                                    |
| [detail-syntax](#check_nscp_version_detail-syntax) | ${version} (${date}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_nscp_version_perf-syntax)     | version              | Performance alias syntax.                                                                                        |



<h5 id="check_nscp_version_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_nscp_version_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_nscp_version_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_nscp_version_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_nscp_version_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_nscp_version_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_nscp_version_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_nscp_version_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_nscp_version_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_nscp_version_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${version} (${date})`

<h5 id="check_nscp_version_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `version`


<a id="check_nscp_version_filter_keys"></a>
#### Filter keywords


| Option  | Description                                                                |
|---------|----------------------------------------------------------------------------|
| build   | The build (the 3 in 0.1.2.3) not available in release versions after 0.6.0 |
| date    | The NSClient++ Build date                                                  |
| major   | The major (the 1 in 0.1.2.3)                                               |
| minor   | The minor (the 2 in 0.1.2.3)                                               |
| release | The release (the 0 in 0.1.2.3)                                             |
| version | The NSClient++ Version as a string                                         |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |




## Configuration



| Path / Section                               | Description  |
|----------------------------------------------|--------------|
| [/settings/nscp/check/update](#update-check) | Update check |



### Update check <a id="/settings/nscp/check/update"></a>

Configuration for the check_nscp_update command which checks GitHub for newer NSClient++ releases.




| Key                                         | Default Value                                     | Description             |
|---------------------------------------------|---------------------------------------------------|-------------------------|
| [ca](#ca-bundle)                            | C:\src\build\nscp/security/windows-ca.pem         | CA bundle               |
| [cache hours](#cache-duration)              | 24                                                | Cache duration          |
| [check experimental](#include-pre-releases) | false                                             | Include pre-releases    |
| [tls version](#minimum-tls-version)         | tlsv1.2+                                          | Minimum TLS version     |
| [url](#update-url)                          | https://api.github.com/repos/mickem/nscp/releases | Update URL              |
| [verify mode](#certificate-verify-mode)     | peer                                              | Certificate verify mode |



```ini
# Configuration for the check_nscp_update command which checks GitHub for newer NSClient++ releases.
[/settings/nscp/check/update]
ca=C:\src\build\nscp/security/windows-ca.pem
cache hours=24
check experimental=false
tls version=tlsv1.2+
url=https://api.github.com/repos/mickem/nscp/releases
verify mode=peer

```





#### CA bundle <a id="/settings/nscp/check/update/ca"></a>

Path to a CA bundle used to verify the update endpoint certificate. Defaults to the trusted system CA store; point at a private bundle when running behind a TLS-inspecting proxy.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | ca                                                          |
| Default value: | `C:\src\build\nscp/security/windows-ca.pem`                 |


**Sample:**

```
[/settings/nscp/check/update]
# CA bundle
ca=C:\src\build\nscp/security/windows-ca.pem
```



#### Cache duration <a id="/settings/nscp/check/update/cache hours"></a>

Number of hours to cache the latest version lookup. The GitHub API is queried at most once per cache window to avoid rate limits.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | cache hours                                                 |
| Default value: | `24`                                                        |


**Sample:**

```
[/settings/nscp/check/update]
# Cache duration
cache hours=24
```



#### Include pre-releases <a id="/settings/nscp/check/update/check experimental"></a>

When true, GitHub pre-releases (experimental builds) are also considered when determining the latest available version. When false (default) only stable releases are considered.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | check experimental                                          |
| Default value: | `false`                                                     |


**Sample:**

```
[/settings/nscp/check/update]
# Include pre-releases
check experimental=false
```



#### Minimum TLS version <a id="/settings/nscp/check/update/tls version"></a>

Minimum TLS protocol version accepted when fetching the GitHub releases endpoint. Defaults to tlsv1.2+ which permits TLS 1.2 and TLS 1.3 only. Allowed values: tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | tls version                                                 |
| Default value: | `tlsv1.2+`                                                  |


**Sample:**

```
[/settings/nscp/check/update]
# Minimum TLS version
tls version=tlsv1.2+
```



#### Update URL <a id="/settings/nscp/check/update/url"></a>

Base URL of the GitHub releases API used to look up the latest NSClient++ version. Point this at a mirror or internal proxy when running in environments without direct GitHub access.





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | url                                                         |
| Default value: | `https://api.github.com/repos/mickem/nscp/releases`         |


**Sample:**

```
[/settings/nscp/check/update]
# Update URL
url=https://api.github.com/repos/mickem/nscp/releases
```



#### Certificate verify mode <a id="/settings/nscp/check/update/verify mode"></a>

TLS certificate verification mode applied to the update endpoint. Defaults to 'peer' so the server certificate chain is validated against the configured CA bundle. Set to 'none' to disable verification (not recommended).





| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/nscp/check/update](#/settings/nscp/check/update) |
| Key:           | verify mode                                                 |
| Default value: | `peer`                                                      |


**Sample:**

```
[/settings/nscp/check/update]
# Certificate verify mode
verify mode=peer
```


