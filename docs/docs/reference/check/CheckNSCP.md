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

#### About `check_nscp`

`check_nscp` reports on the health of the agent itself: whether it has crashed,
whether it has logged errors, and how long it has been running. It is the check
you point at NSClient++ to answer "is my monitoring agent healthy?", as opposed
to `check_nscp_version` (which version is this?) and `check_nscp_update` (is a
newer one available?).

It is a normal filter check, so the usual `filter` / `warning` / `critical`,
`top-syntax` / `detail-syntax` and `perf-config` options all apply. Exactly one
object is fed to the filter — the agent — so `${list}` is a single line.

The default thresholds keep the historical verdict: **any** crash report or
**any** logged error makes the agent CRITICAL.

```
check_nscp
OK: 0 crash(es), 0 error(s), uptime 0
```

#### What the keywords are measured against

| Keyword | Where it comes from |
|---------|---------------------|
| `crashes`, `last_crash`, `crash_age` | The crash archive folder, i.e. the `archive folder` key in `[/settings/crash]`. Files ending in `.crash`, `.dmp` or `.txt` count as crash reports. |
| `errors`, `last_error` | Messages logged at ERROR or CRITICAL level, counted since the module was loaded. The counter is not reset by a restart of the check, only by a restart of the agent. |
| `uptime` | Time since the CheckNSCP module was loaded, which for a normally configured agent is the agent's own uptime. |
| `version`, `date` | The running build, same values `check_nscp_version` reports. |

The three crash-report extensions are historical. 0.6.10 and later write one
plain-text `<timestamp>.crash` per crash, naming the exception, the faulting
address and the module it landed in. Up to 0.6.9 the agent used Google
Breakpad, which left a `<guid>.dmp` minidump and a `<guid>.dmp.txt` description
behind; breakpad was dropped because its vendored submodule and build machinery
had become a dependency burden. All three extensions are counted, so an archive
that predates the change still reports correctly.

`uptime` and `crash_age` accept units in thresholds, so you can write
`crit=uptime < 5m` or `warn=crash_age < 7d` rather than converting to seconds
yourself. Rendered through `${uptime}` / `${crash_age}` they come out as a
human-readable duration whose largest unit is controlled by `max-unit`.

#### Crash reports are a Windows concept

NSClient++ only archives crash reports on Windows — there is no crash handler in
the Unix builds. On Linux the crash archive folder therefore stays empty and
`crashes` is always `0`; that is the documented result, not a failure. The
`errors` and `uptime` keywords work identically on both platforms.

#### Alerting on a crash that has since been cleaned up

`crashes` counts every report still in the archive folder, so an agent that
crashed a year ago and was never tidied up stays CRITICAL forever. To alert only
on a *recent* crash, threshold on `crash_age` instead and let the count alone go
unremarked:

```
check_nscp "crit=crash_age < 7d" "warn=errors > 0"
```

`crash_age` is `none` when the archive holds no report at all, and every numeric
comparison against `none` is false — so the expression above is quietly OK on an
agent that has never crashed.

**Jump to section:**

* [Sample Commands](#check_nscp_samples)
* [Command-line Arguments](#check_nscp_options)
* [Filter keywords](#check_nscp_filter_keys)


<a id="check_nscp_samples"></a>
#### Sample Commands

**Check that the agent is healthy**

With no arguments the defaults apply: any crash report or any logged error is
CRITICAL.

```
check_nscp
OK: 0 crash(es), 0 error(s), uptime 4d 02:17|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```

**An agent that has crashed**

```
check_nscp
CRITICAL: 2 crash(es), 0 error(s), uptime 03:12|'nscp_crashes'=2;0;0 'nscp_errors'=0;0;0
```

**Name the crash report and how old it is**

```
check_nscp "detail-syntax=last=${last_crash} age=${crash_age}"
CRITICAL: last=2026-01-02-08-15-31.crash age=0:31|'nscp_crashes'=2;0;0 'nscp_errors'=0;0;0
```

**Only alert on a recent crash**

`crash_age` accepts units, so the window is written the way you think about it.
An agent that crashed half an hour ago trips a 7-day window:

```
check_nscp "crit=crash_age < 7d"
CRITICAL: 2 crash(es), 0 error(s), uptime 03:12|'nscp_crash_age'=1878s;0;604800
```

…and one that has never crashed does not, because every numeric comparison
against `none` is false. There is no value to graph either, so the check emits
no performance data at all:

```
check_nscp "crit=crash_age < 7d"
OK: 0 crash(es), 0 error(s), uptime 4d 02:17
```

**Warn before the crash archive piles up**

A threshold on a keyword also produces that keyword's performance data, so this
graphs the crash count with both thresholds attached — and nothing else.

```
check_nscp "warn=crashes > 5" "crit=crashes > 10"
OK: 0 crash(es), 0 error(s), uptime 4d 02:17|'nscp_crashes'=0;5;10
```

**Alert on a freshly restarted agent**

`uptime` takes units too. The default `critical` still applies here, so the
crash and error metrics come along with the uptime one.

```
check_nscp "warn=uptime < 5m"
WARNING: 0 crash(es), 0 error(s), uptime 42s|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0 'nscp_uptime'=42s;300;0
```

**Change the granularity of the rendered durations**

`max-unit` controls the largest unit `${uptime}` and `${crash_age}` use.

```
check_nscp "max-unit=h" "detail-syntax=up=${uptime}"
OK: up=98:17|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```

**Report the last error the agent logged**

`errors` counts messages logged at ERROR or CRITICAL level since the agent
started; `${last_error}` is the most recent of them.

```
check_nscp "detail-syntax=${errors} error(s): ${last_error}"
CRITICAL: 1 error(s): Failed to load module: CheckWMI|'nscp_crashes'=0;0;0 'nscp_errors'=1;0;0
```

**Custom top-level output**

```
check_nscp "top-syntax=agent is ${status}"
agent is OK|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```



<a id="check_nscp_options"></a>
#### Command-line Arguments

| Option                           | Default Value | Description                                                                                                                                               |
|----------------------------------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------|
| [max-unit](#check_nscp_max-unit) | w             | Largest time unit used to render ${uptime} and ${crash_age}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'. |



<h5 id="check_nscp_max-unit">max-unit:</h5>

Largest time unit used to render ${uptime} and ${crash_age}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'.

*Default Value:* `w`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                     | Default Value                                              |
|------------------------------------------------------------------------------------------------------------|------------------------------------------------------------|
| <a id="check_nscp_filter"></a>[filter](../common-options.md#filter)                                        |                                                            |
| <a id="check_nscp_warning"></a>[warning](../common-options.md#warning)                                     |                                                            |
| <a id="check_nscp_warn"></a>[warn](../common-options.md#warn)                                              |                                                            |
| <a id="check_nscp_critical"></a>[critical](../common-options.md#critical)                                  | crashes > 0 or errors > 0                                  |
| <a id="check_nscp_crit"></a>[crit](../common-options.md#crit)                                              |                                                            |
| <a id="check_nscp_ok"></a>[ok](../common-options.md#ok)                                                    |                                                            |
| <a id="check_nscp_debug"></a>[debug](../common-options.md#debug)                                           | false                                                      |
| <a id="check_nscp_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                      |
| <a id="check_nscp_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                    |
| <a id="check_nscp_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                            |
| <a id="check_nscp_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                      |
| <a id="check_nscp_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                          |
| <a id="check_nscp_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                         |
| <a id="check_nscp_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                            |
| <a id="check_nscp_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                            |
| <a id="check_nscp_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${crashes} crash(es), ${errors} error(s), uptime ${uptime} |
| <a id="check_nscp_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | nscp                                                       |
| <a id="check_nscp_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                            |
| <a id="check_nscp_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                            |
| <a id="check_nscp_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                         |
| <a id="check_nscp_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nscp_filter_keys"></a>
#### Filter keywords

| Option     | Description                                                                                                             |
|------------|-------------------------------------------------------------------------------------------------------------------------|
| crash_age  | Seconds since the most recent crash report was written, 'none' when nothing has crashed (accepts units: crash_age < 7d) |
| crashes    | Number of crash reports found in the crash archive folder                                                               |
| date       | The build date of the running NSClient++                                                                                |
| errors     | Number of errors logged by NSClient++ since it was started                                                              |
| last_crash | File name of the most recent crash report (empty when nothing has crashed)                                              |
| last_error | The most recently logged error message (empty when nothing has been logged)                                             |
| uptime     | Seconds since NSClient++ was started (accepts units: uptime < 5m)                                                       |
| version    | The running NSClient++ version                                                                                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nscp_update

Check if there is a newer version of NSClient++ available on GitHub. The result is cached (default 24 hours) to avoid hitting the GitHub API rate limit.

#### About `check_nscp_update`

`check_nscp_update` compares the running build against the latest release
published on the NSClient++ GitHub releases page, so a fleet can alert on
"this agent is out of date" without an external inventory.

The comparison is exposed as filter keywords: `update_available` (0/1),
`versions_behind`, and the latest release's `latest_version` /
`latest_major` / `latest_minor` / `latest_release` / `latest_build` alongside
the running `version` / `major` / `minor` / `release` / `build`. `tag`,
`published` and `url` identify the release itself, so the alert can point an
operator straight at the download.

Both the default warning and the default critical threshold are
`update_available = 1`, so a bare call reports CRITICAL as soon as any newer
release exists. Most fleets want something less noisy — for example only caring
about falling more than one release behind:

```
check_nscp_update "warn=versions_behind > 0" "crit=versions_behind > 1"
```

**The result is cached** (24 hours by default) because the GitHub API is rate
limited per source IP, and a fleet of agents checking on every poll would
exhaust that budget quickly. The cache means the check is cheap to schedule
often, but also that it can lag a fresh release by up to the cache lifetime.

**This check makes an outbound HTTPS request to github.com.** On a host with no
egress — which is the normal case for a monitored server — it cannot work;
either allow that one destination or run the check from a single management host
rather than fleet-wide.

When the request fails, the reason lands in the `error` keyword — but the
**status stays OK**, because `update_available` is 0 and that is all the default
thresholds look at. An agent that cannot reach GitHub therefore reports exactly
the same thing as an agent that is up to date. If you rely on this check, say so
explicitly:

```
check_nscp_update "warn=update_available = 1" "crit=error != ''"
```

**Jump to section:**

* [Sample Commands](#check_nscp_update_samples)
* [Command-line Arguments](#check_nscp_update_options)
* [Filter keywords](#check_nscp_update_filter_keys)


<a id="check_nscp_update_samples"></a>
#### Sample Commands

**Default check (any newer release is CRITICAL):**

Both the default warning and the default critical threshold are
`update_available = 1`.

```
check_nscp_update
CRITICAL: 0.17.2 (latest: 0.18.1)|'version_update_available'=1;1;1
```

An agent on the current release:

```
check_nscp_update
OK: 0.18.1 (latest: 0.18.1)|'version_update_available'=0;1;1
```

**Something less noisy — only alert on falling more than one release behind:**

```
check_nscp_update "warn=versions_behind > 0" "crit=versions_behind > 1"
OK: 0.18.1 (latest: 0.18.1)|'version_versions_behind'=0;0;1
```

**Point an operator at the release:**

```
check_nscp_update "detail-syntax=${version} -> ${latest_version} (${tag}, published ${published}) ${url}"
CRITICAL: 0.17.2 -> 0.18.1 (0.18.1, published 2026-08-14) https://github.com/mickem/nscp/releases/tag/0.18.1
```

**When GitHub cannot be reached:**

The failure lands in `error`, but `update_available` stays 0 — so with the
default thresholds a blocked agent reports **exactly the same OK** as an
up-to-date one:

```
check_nscp_update "detail-syntax=avail=${update_available} latest=${latest_version} err=${error}"
OK: avail=0 latest= err=HTTP 426 from https://api.github.com/repos/mickem/nscp/releases/latest|'version_update_available'=0;1;1
```

Threshold on `error` explicitly if you rely on this check:

```
check_nscp_update "warn=update_available = 1" "crit=error != ''"
CRITICAL: 0.18.1 (latest: )|'version_update_available'=0;1;0
```

**Caching:**

The GitHub result is cached (24 hours by default) to stay inside the API's rate
limit, so the check is cheap to schedule often — but can lag a fresh release by
up to the cache lifetime.



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

#### About `check_nscp_version`

`check_nscp_version` reports the version of the running NSClient++ build. It
decomposes the version so that thresholds can be written against it rather than
against a string: `release`, `major`, `minor` and `build` are numeric, while
`version` and `date` carry the rendered string and the build date.

The typical use is fleet hygiene — flag agents that have fallen behind a version
you have standardised on:

```
check_nscp_version "crit=major < 12"
```

There are no default thresholds, so a bare call is always OK and simply reports
the version. That makes it a useful heartbeat probe as well: the cheapest way to
confirm an agent is up and answering.

Note that `build` is only meaningful on builds before 0.6.0; newer releases
report `0` for it.

If you want to know whether a *newer* release exists rather than which one is
installed, use [`check_nscp_update`](#check_nscp_update), which compares against
the latest release published on GitHub. For the running agent's own health —
crashes, logged errors, uptime — see [`check_nscp`](#check_nscp).

**Jump to section:**

* [Sample Commands](#check_nscp_version_samples)
* [Command-line Arguments](#check_nscp_version_options)
* [Filter keywords](#check_nscp_version_filter_keys)


<a id="check_nscp_version_samples"></a>
#### Sample Commands

**Report the running version:**

```
check_nscp_version
OK: 0.18.1 (2026-08-14)
```

**Alert when an agent has fallen behind a version you standardised on:**

The parts are numeric, so they threshold directly. `version` is
`release.major.minor`, so 0.18.1 is release 0, major 18, minor 1.

```
check_nscp_version "crit=major < 12"
CRITICAL: 0.11.4 (2024-03-09)|'version_major'=11;0;12
```

An agent that is current stays OK:

```
check_nscp_version "crit=major < 12"
OK: 0.18.1 (2026-08-14)|'version_major'=18;0;12
```

**Inspect all the parts:**

```
check_nscp_version "detail-syntax=${version} major=${major} minor=${minor} release=${release} build=${build}"
OK: 0.18.1 major=18 minor=1 release=0 build=0
```

`build` is only meaningful on builds before 0.6.0; newer releases report `0`.

**As a heartbeat probe:**

There are no default thresholds, so a bare call is always OK — anything else
means the agent is not answering.

```
check_nrpe --host 192.168.56.103 --command check_nscp_version
OK: 0.18.1 (2026-08-14)
```



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
