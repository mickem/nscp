# CheckLogFile

File for checking log files and various other forms of updating text files

## Enable module

To enable this module and and allow using the commands you need to ass `CheckLogFile = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckLogFile = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckLogFile module.

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

### check_logfile

Check for errors in log file or generic pattern matching in text files.

#### Only checking new lines (`bookmark`)

By default `check_logfile` reads the **entire** file on every run, so a single
`ERROR` line keeps failing the check for as long as it stays in the file. Add
`bookmark` to make the check incremental instead: NSClient++ remembers how far
it read last time and each run only looks at what was appended since.

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors
```

The name (`app-errors` above) identifies the stored position. Two checks that
use the same name over the same file share one position — the first one to run
consumes the lines. Use distinct names when two checks look for different
things in the same file, or let the name be derived automatically by passing
`bookmark` with no value (`bookmark`, `bookmark=` and `bookmark=auto` all mean
the same thing), which builds it from the file name plus a hash of the
`filter`, `warning` and `critical` expressions:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark
```

Positions are kept per file, so `file=` may be repeated as usual; each file is
tracked on its own. They are written to `${data-path}/nsclient.db` when
NSClient++ shuts down and restored on start, so a restart does not re-report
everything.

Because an automatic name covers the expressions, editing the filter starts a
new position (and the file is read in full once more) and leaves the old one
behind in the store. Prefer an explicit name for checks whose filter changes
often, or which are generated with varying arguments.

Behaviour worth knowing:

* **The first check of a file reads it in full.** Only from the second check on
  is the check incremental. This is deliberate — entries that were already in
  the file when monitoring started are reported rather than silently dropped.
* **An unterminated last line is held back.** If the file ends mid-line (the
  writer has not written the `line-split` terminator yet) that fragment is not
  reported, and the position stays in front of it. The line is reported once,
  in full, by the check that sees its terminator. Without a bookmark the
  fragment is reported as-is, and again once it is complete.
* **Rotation and truncation are detected.** If the file is shorter than the
  stored position, or the first bytes of the file no longer match what was
  there before (a rotated or re-created file, `copytruncate`, a daily file
  under a fixed name), the file is read from the beginning again. Rotation to a
  *different* name is not followed: point the check at the name that keeps
  receiving new lines.
* **A failing check moves nothing.** If any of the files cannot be read the
  check returns an error and every position stays where it was, so the lines
  which were read on the way are reported by the next successful check instead
  of vanishing with the error.
* **A quiet check is OK, not UNKNOWN.** When nothing new arrived the result is
  the empty state (`%(status): Nothing found`), which is OK by default; use
  `empty-state=` to change it.
* **Checks without `bookmark` are unaffected.** They neither read nor advance
  any stored position, so an ad-hoc full scan can be run at any time without
  disturbing a bookmarked check.

Before you switch a check over to `bookmark`, know what you are trading a
re-reported line for:

* **A line is consumed when the check runs, not when its result arrives.** With
  a bookmark the position moves as soon as the lines have been read. If the
  result is submitted passively (NSCA, NRDP, …) and that submission fails, the
  lines it described are not reported again by the next check. An actively
  polled check is not exposed to this: the position moves as the poller
  receives the answer.
* **Positions are saved when NSClient++ shuts down.** A crash, a killed
  service or a power loss therefore rewinds every bookmark to where it was when
  the service last stopped cleanly, and the lines written since are reported
  again. They are never lost, only repeated.
* **A last line without its terminator is never reported on its own.** This is
  the flip side of holding back half-written lines: a file which is written in
  one go and does not end with `line-split` keeps its final line unreported
  until something appends to the file. Without a bookmark that line is reported
  on every check as before.
* **`${total}` counts what the check looked at.** With a bookmark that is the
  lines added since the previous run — not the number of lines in the file.
  `${count}` is, as always, how many of them matched the filter.
* **The number of remembered positions is capped** (at 1000 file/bookmark
  pairs, which no ordinary configuration comes close to). If a host generates
  bookmark names — a script which puts a timestamp in the name, or automatic
  names for a filter which keeps changing — the least recently used positions
  are dropped, and the files they tracked are read in full the next time that
  name shows up. A dropped position is also cleared from `nsclient.db` on the
  next shutdown, so the store does not grow forever.

Real-time monitoring (`/settings/logfile/real-time/checks`) is the other way to
get each line reported once; it pushes results as lines are written instead of
being polled. `bookmark` is the polled equivalent and needs no configuration on
the agent.

#### Only checking the newest lines (`max-lines`, `newest`)

`max-lines=N` limits the check to the newest `N` lines of each file. Everything
else follows from that: `${total}` counts what was examined, not what the file
holds, and only the selected lines can match the filter.

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" max-lines=100
```

The limit is per file, so a check with several `file=` arguments takes the
newest `N` lines of each of them.

`newest` says which end of the file the newest line is at:

| Value  | Meaning                                                                       |
|--------|-------------------------------------------------------------------------------|
| `last` | Lines are appended (the default, and what almost every machine-written log does). `max-lines` takes them from the end of the file. |
| `first`| The file is rewritten with the newest line at the top, as hand-maintained changelogs often are. `max-lines` takes them from the start of the file. |

Whichever end they come from, the selected lines are reported in the order they
appear in the file, so `%(list)` reads the way the file does.

Behaviour worth knowing:

* **Only the wanted part of the file is read.** The check seeks to the newest
  `N` lines instead of loading the whole file, so `max-lines` is a cheap way to
  look at the tail of a very large log. (A `line-split` value which can overlap
  itself, such as `aaa` or `--`, cannot be located from the end; those files are
  read in full and the surplus lines are dropped afterwards. The result is the
  same either way.)
* **`max-lines` combines with `bookmark`.** The bookmark still decides which
  lines are new, and the limit then caps how many of them are reported — useful
  when an application can dump thousands of lines at once. The lines dropped by
  the limit are *consumed*: they are not reported by a later check either, since
  the stored position moves past everything that was read.
* **`newest=first` cannot be combined with `bookmark`.** A file which is
  rewritten from the top has no stable position to resume from — its first bytes
  change on every write, so the bookmark would detect a "new" file and re-read
  it in full every single time. The check reports this as an error rather than
  doing it quietly.
* **An unterminated last line still counts as a line.** Without a bookmark the
  trailing fragment is one of the `N`; with a bookmark it is held back as usual.

**Jump to section:**

* [Sample Commands](#check_logfile_samples)
* [Command-line Arguments](#check_logfile_options)
* [Filter keywords](#check_logfile_filter_keys)


<a id="check_logfile_samples"></a>
#### Sample Commands

**Find errors in a log file**

Given `/var/log/app.log`:

```
app started
ERROR failed to connect to db
INFO retrying
ERROR failed to connect to db
```

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" "critical=count > 3"
2/4 (ERROR failed to connect to db, ERROR failed to connect to db)|'count'=2;0;3
```

`${count}` is the number of matching lines, `${total}` the number of lines read.

**Only report lines added since the last check (`bookmark`)**

The first check reads the whole file:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors
2/4 (ERROR failed to connect to db, ERROR failed to connect to db)|'count'=2;0;0
```

Running it again with nothing appended reports nothing — the two errors are not
raised over and over:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors
OK: Nothing found|'count'=0;0;0
```

After `ERROR disk full` is appended, only that line is considered:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors
1/1 (ERROR disk full)|'count'=1;0;0
```

**Let the bookmark name itself be derived**

`bookmark` without a value derives the name from the file and the filter /
warning / critical expressions, which keeps this check from consuming the lines
of the `app-errors` check above (it starts from the beginning, since this name
has not seen the file before):

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark
3/5 (ERROR failed to connect to db, ERROR failed to connect to db, ERROR disk full)|'count'=3;0;0
```

`bookmark=` (an empty value) and `bookmark=auto` are spelled differently by
different transports but mean exactly this.

**Watch several files with one check**

Each file keeps its own position; the counts are aggregated. A fresh bookmark
name starts by reading both files in full:

```
check_logfile file=/var/log/app.log file=/var/log/worker.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=all-errors
4/7 (ERROR failed to connect to db, ERROR failed to connect to db, ERROR disk full, ERROR queue stalled)|'count'=4;0;0
```

Afterwards only the file that actually grew contributes:

```
check_logfile file=/var/log/app.log file=/var/log/worker.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=all-errors
1/1 (ERROR queue stalled again)|'count'=1;0;0
```

**Only look at the newest lines (`max-lines`)**

Given `/var/log/app.log`:

```
app started
ERROR failed to connect to db
INFO retrying
ERROR failed to connect to db
INFO recovered
ERROR disk full
```

Without a limit every line is read:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0"
3/6 (ERROR failed to connect to db, ERROR failed to connect to db, ERROR disk full)|'count'=3;0;0
```

With `max-lines=3` only the last three lines are examined — `${total}` drops to
3 and the older error is out of scope:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" max-lines=3
2/3 (ERROR failed to connect to db, ERROR disk full)|'count'=2;0;0
```

**Files whose newest line is at the top (`newest=first`)**

Given a hand-maintained `/var/log/deploy-changelog.txt` where each new entry is
added at the top:

```
2018-07-20 deploy 42 FAILED rollback started
2018-07-19 deploy 41 ok
2018-07-18 deploy 40 ok
2018-07-17 deploy 39 FAILED disk full
```

`newest=first` makes `max-lines` count from the top of the file, so only the two
most recent deploys are checked:

```
check_logfile file=/var/log/deploy-changelog.txt "filter=column1 like 'FAILED'" "warning=count > 0" max-lines=2 newest=first
1/2 (2018-07-20 deploy 42 FAILED rollback started)|'count'=1;0;0
```

Without the limit `newest=first` changes nothing — the whole file is read either
way:

```
check_logfile file=/var/log/deploy-changelog.txt "filter=column1 like 'FAILED'" "warning=count > 0" newest=first
2/4 (2018-07-20 deploy 42 FAILED rollback started, 2018-07-17 deploy 39 FAILED disk full)|'count'=2;0;0
```

**Report a quiet check as something other than OK**

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors empty-state=unknown
UNKNOWN: Nothing found|'count'=0;0;0
```



<a id="check_logfile_options"></a>
#### Command-line Arguments

<a id="check_logfile_split"></a>
<a id="check_logfile_files"></a>

| Option                                      | Default Value | Description                                                                                                                                                                                                                                               |
|---------------------------------------------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [line-split](#check_logfile_line-split)     | \n            | Character string used to split a file into several lines (default `\n`).                                                                                                                                                                                  |
| [column-split](#check_logfile_column-split) | \t            | Character string to split a line into several columns (default \t)                                                                                                                                                                                        |
| split                                       |               | Alias for split-column                                                                                                                                                                                                                                    |
| [file](#check_logfile_file)                 |               | File to read (can be specified multiple times to check multiple files.                                                                                                                                                                                    |
| files                                       |               | A comma separated list of files to scan (same as file except a list)                                                                                                                                                                                      |
| [bookmark](#check_logfile_bookmark)         | auto          | Only scan lines added since the last check with the same bookmark name.                                                                                                                                                                                   |
| [max-lines](#check_logfile_max-lines)       | 0             | Only examine the newest <N> lines of each file (0, the default, means every line).                                                                                                                                                                        |
| [newest](#check_logfile_newest)             | last          | Which end of the file holds the newest line: `last` (the default: lines are appended, as with most machine-written logs) or `first` (the file is rewritten with the newest line at the top, which is common in hand-maintained files such as changelogs). |



<h5 id="check_logfile_line-split">line-split:</h5>

Character string used to split a file into several lines (default `\n`).
The escape sequences `\n` and `\t` are translated to LF and TAB respectively; all other characters are taken literally. Multi-character delimiters are supported (for example `\r\n` to split strictly on CRLF, or `|||` for a  custom separator). Setting `line-split` to an empty value (`line-split=`) makes the entire file content available as a single record, which is useful together with a multi-line regular-expression filter.\nWhen the chosen delimiter ends with `
`, a trailing carriage return is stripped from each record so that files with CRLF line endings produce clean lines.

*Default Value:* `\n`

<h5 id="check_logfile_column-split">column-split:</h5>

Character string to split a line into several columns (default \t)

*Default Value:* `\t`

<h5 id="check_logfile_file">file:</h5>

File to read (can be specified multiple times to check multiple files.
Notice that specifying multiple files will create an aggregate set it will not check each file individually.
In other words if one file contains an error the entire check will result in error or if you check the count it is the global count which is used.


<h5 id="check_logfile_bookmark">bookmark:</h5>

Only scan lines added since the last check with the same bookmark name.
NSClient++ remembers, per file and per bookmark, how far it read last time and resumes from there, so a line is reported once instead of on every check. The first check of a file reads it in full; a file which is truncated, rotated or replaced is detected (via its size and a fingerprint of its first bytes) and read from the beginning again. A trailing line which is not yet terminated by line-split is held back until it is complete, so half-written lines are never reported twice.
If you set this to auto (or leave the value empty) the bookmark name is derived from the file name together with a hash of your filter, warning and critical expressions, which keeps unrelated checks of the same file from consuming each other's lines. Use an explicit name to share (or separate) positions deliberately. Positions are persisted when NSClient++ shuts down and restored on start; the newest ones are kept if more than a thousand accumulate.

*Default Value:* `auto`

<h5 id="check_logfile_max-lines">max-lines:</h5>

Only examine the newest <N> lines of each file (0, the default, means every line).
The limit is applied per file, after any bookmark: with a bookmark the check still only sees lines added since the last check, and this caps how many of them are reported when a burst of lines was written at once. The lines dropped by the limit are never reported later either - the bookmark moves past everything which was read.
Which end of the file holds the newest lines is controlled by `newest`.

*Default Value:* `0`

<h5 id="check_logfile_newest">newest:</h5>

Which end of the file holds the newest line: `last` (the default: lines are appended, as with most machine-written logs) or `first` (the file is rewritten with the newest line at the top, which is common in hand-maintained files such as changelogs).
This only decides which end `max-lines` counts from; lines are always reported in the order they appear in the file. `newest=first` cannot be combined with `bookmark`, since a file which is rewritten from the top has no stable position to resume from.

*Default Value:* `last`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                         | Default Value                       |
|------------------------------------------------------------------------------------------------|-------------------------------------|
| <a id="check_logfile_filter"></a>[filter](../common-options.md#filter)                         |                                     |
| <a id="check_logfile_warning"></a>[warning](../common-options.md#warning)                      |                                     |
| <a id="check_logfile_warn"></a>[warn](../common-options.md#warn)                               |                                     |
| <a id="check_logfile_critical"></a>[critical](../common-options.md#critical)                   |                                     |
| <a id="check_logfile_crit"></a>[crit](../common-options.md#crit)                               |                                     |
| <a id="check_logfile_ok"></a>[ok](../common-options.md#ok)                                     |                                     |
| <a id="check_logfile_debug"></a>[debug](../common-options.md#debug)                            | false                               |
| <a id="check_logfile_show-all"></a>[show-all](../common-options.md#show-all)                   | false                               |
| <a id="check_logfile_empty-state"></a>[empty-state](../common-options.md#empty-state)          | ignored                             |
| <a id="check_logfile_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                     |
| <a id="check_logfile_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                               |
| <a id="check_logfile_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                   |
| <a id="check_logfile_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${count}/${total} (${problem_list}) |
| <a id="check_logfile_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                |                                     |
| <a id="check_logfile_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): Nothing found            |
| <a id="check_logfile_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${column1}                          |
| <a id="check_logfile_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${column1}                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_logfile_filter_keys"></a>
#### Filter keywords

| Option   | Description                                   |
|----------|-----------------------------------------------|
| column() | Fetch the value from the given column number. |
| column1  | The value in the first column                 |
| column2  | The value in the second column                |
| column3  | The value in the third column                 |
| column4  | The value in the 4:th column                  |
| column5  | The value in the 5:th column                  |
| column6  | The value in the 6:th column                  |
| column7  | The value in the 7:th column                  |
| column8  | The value in the 8:th column                  |
| column9  | The value in the 9:th column                  |
| file     | The name of the file                          |
| filename | The name of the file                          |
| line     | Match the content of an entire line           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                           | Description         |
|----------------------------------------------------------|---------------------|
| [/settings/logfile/real-time](#real-time-filtering)      | Real-time filtering |
| [/settings/logfile/real-time/checks](#real-time-filters) | Real-time filters   |


### Real-time filtering <a id="/settings/logfile/real-time"></a>

A set of options to configure the real time checks

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [enabled](#real-time) | false         | Real time   |


```ini
# A set of options to configure the real time checks
[/settings/logfile/real-time]
enabled=false
```

#### Real time <a id="/settings/logfile/real-time/enabled"></a>

Spawns a background thread which waits for file changes.


| Key            | Description                                                 |
|----------------|-------------------------------------------------------------|
| Path:          | [/settings/logfile/real-time](#/settings/logfile/real-time) |
| Key:           | enabled                                                     |
| Default value: | `false`                                                     |


**Sample:**

```
[/settings/logfile/real-time]
# Real time
enabled=false
```

### Real-time filters <a id="/settings/logfile/real-time/checks"></a>

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
| list separator   |                           | LIST SEPARATOR   |
| maximum age      | 5m                        | MAXIMUM AGE      |
| ok               |                           | OK FILTER        |
| ok syntax        |                           | SYNTAX           |
| perf config      |                           | PERF CONFIG      |
| read entire file |                           | read entire file |
| severity         |                           | SEVERITY         |
| silent period    | false                     | Silent period    |
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
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#read entire file=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#top syntax=...
#warning=...

```





