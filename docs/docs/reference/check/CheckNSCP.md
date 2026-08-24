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



<a id="check_nscp_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### check_nscp_update

Check if there is a newer version of NSClient++ available on GitHub. The result is cached (default 24 hours) to avoid hitting the GitHub API rate limit.

**Jump to section:**

* [Command-line Arguments](#check_nscp_update_options)
* [Filter keywords](#check_nscp_update_filter_keys)



<a id="check_nscp_update_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                            | Default Value                          |
|-------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| <a id="check_nscp_update_filter"></a>[filter](../common-options.md#filter)                                        |                                        |
| <a id="check_nscp_update_warning"></a>[warning](../common-options.md#warning)                                     | update_available = 1                   |
| <a id="check_nscp_update_warn"></a>[warn](../common-options.md#warn)                                              |                                        |
| <a id="check_nscp_update_critical"></a>[critical](../common-options.md#critical)                                  | update_available = 1                   |
| <a id="check_nscp_update_crit"></a>[crit](../common-options.md#crit)                                              |                                        |
| <a id="check_nscp_update_ok"></a>[ok](../common-options.md#ok)                                                    |                                        |
| <a id="check_nscp_update_debug"></a>[debug](../common-options.md#debug)                                           | false                                  |
| <a id="check_nscp_update_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                  |
| <a id="check_nscp_update_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                |
| <a id="check_nscp_update_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                        |
| <a id="check_nscp_update_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                  |
| <a id="check_nscp_update_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                      |
| <a id="check_nscp_update_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                     |
| <a id="check_nscp_update_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                        |
| <a id="check_nscp_update_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                        |
| <a id="check_nscp_update_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${version} (latest: ${latest_version}) |
| <a id="check_nscp_update_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | version                                |
| <a id="check_nscp_update_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                        |
| <a id="check_nscp_update_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                        |
| <a id="check_nscp_update_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                     |
| <a id="check_nscp_update_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nscp_version

Check the version of NSClient++ which is used.

**Jump to section:**

* [Command-line Arguments](#check_nscp_version_options)
* [Filter keywords](#check_nscp_version_filter_keys)



<a id="check_nscp_version_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value        |
|--------------------------------------------------------------------------------------------------------------------|----------------------|
| <a id="check_nscp_version_filter"></a>[filter](../common-options.md#filter)                                        |                      |
| <a id="check_nscp_version_warning"></a>[warning](../common-options.md#warning)                                     |                      |
| <a id="check_nscp_version_warn"></a>[warn](../common-options.md#warn)                                              |                      |
| <a id="check_nscp_version_critical"></a>[critical](../common-options.md#critical)                                  |                      |
| <a id="check_nscp_version_crit"></a>[crit](../common-options.md#crit)                                              |                      |
| <a id="check_nscp_version_ok"></a>[ok](../common-options.md#ok)                                                    |                      |
| <a id="check_nscp_version_debug"></a>[debug](../common-options.md#debug)                                           | false                |
| <a id="check_nscp_version_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                |
| <a id="check_nscp_version_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored              |
| <a id="check_nscp_version_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                      |
| <a id="check_nscp_version_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                |
| <a id="check_nscp_version_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                    |
| <a id="check_nscp_version_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}   |
| <a id="check_nscp_version_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                      |
| <a id="check_nscp_version_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                      |
| <a id="check_nscp_version_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${version} (${date}) |
| <a id="check_nscp_version_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | version              |
| <a id="check_nscp_version_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                      |
| <a id="check_nscp_version_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                      |
| <a id="check_nscp_version_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                   |
| <a id="check_nscp_version_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                               | Description  |
|----------------------------------------------|--------------|
| [/settings/nscp/check/update](#update-check) | Update check |


### Update check <a id="/settings/nscp/check/update"></a>

Configuration for the check_nscp_update command which checks GitHub for newer NSClient++ releases.

| Key                                         | Default Value                                     | Description             |
|---------------------------------------------|---------------------------------------------------|-------------------------|
| [ca](#ca-bundle)                            | ${ca-path}                                        | CA bundle               |
| [cache hours](#cache-duration)              | 24                                                | Cache duration          |
| [check experimental](#include-pre-releases) | false                                             | Include pre-releases    |
| [tls version](#minimum-tls-version)         | tlsv1.2+                                          | Minimum TLS version     |
| [url](#update-url)                          | https://api.github.com/repos/mickem/nscp/releases | Update URL              |
| [verify mode](#certificate-verify-mode)     | peer                                              | Certificate verify mode |


```ini
# Configuration for the check_nscp_update command which checks GitHub for newer NSClient++ releases.
[/settings/nscp/check/update]
ca=${ca-path}
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
| Default value: | `${ca-path}`                                                |


**Sample:**

```
[/settings/nscp/check/update]
# CA bundle
ca=${ca-path}
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
