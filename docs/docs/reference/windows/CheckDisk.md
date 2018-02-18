# CheckDisk

CheckDisk can check various file and disk related things.

`CheckDisk` is provides two disk related checks one for checking size of drives and the other for checking status of files and folders.

!!! danger
    Please note that UNC and network paths are only available in each session meaning a user mounted share will not be visible to NSClient++ (since services run in their own session).
    But as long as NSClient++ can access the share you can still check it as you specify the UNC path.
    In other words the following will **NOT** work: `check_drivesize drive=m:` But the following will: `check_drivesize drive=\\myserver\\mydrive`



## Queries

A quick reference for all available queries (check commands) in the CheckDisk module.

**List of commands:**

A list of all available queries (check commands)

| Command                             | Description                                       |
|-------------------------------------|---------------------------------------------------|
| [check_drivesize](#check_drivesize) | Check the size (free-space) of a drive or volume. |
| [check_files](#check_files)         | Check various aspects of a file and/or folder.    |
| [checkdrivesize](#checkdrivesize)   | Legacy version of check_drivesize                 |
| [checkfiles](#checkfiles)           | Legacy version of check_drivesize                 |




### check_drivesize

Check the size (free-space) of a drive or volume.

* [Samples](#check_drivesize_samples)

* [Command-line Arguments](#check_drivesize_options)
* [Filter keywords](#check_drivesize_filter_keys)


<a name="check_drivesize_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckDisk_check_drivesize_samples.md)_

To check the size of **the C:\ drive** and **make sure it has atleast 10% free** space::

```
check_drivesize "crit=free<10%" drive=c:
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of **all the drives** and make sure it has at least 10% free space::

```
check_drivesize "crit=free<10%" drive=*
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

To check the size of all the drives and **display all values, not just problems**::

```
check_drivesize drive=* --show-all
L     client CRITICAL: c:: 205GB/223GB used
L     client  Performance data: 'c: free'=18GB;0;22;0;223 'c: free %'=8%;0;9;0;100
```

To check the size of all the drives and **return the value in gigabytes**. By default units on performance data will be scaled to "something appropriate"::

```
check_drivesize "perf-config=*(unit:g)"
L        cli CRITICAL: CRITICAL C:\\: 208.147GB/223.471GB used, D:\\: 399.607GB/465.759GB used
L        cli  Performance data: 'C:\ used'=0.00019g;0.00017;0.00019;0;0.00021 'C:\ used %'=93%;79;89;0;100 'D:\ used'=0.00038g;0.00035;0.00039;0;0.00044 'D:\ used %'=85%;79;89;0;100 'E:\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used'=0g;0;0;0;0 '\\?\Volume{d458535f-27c7-11e4-be66-806e6f6e6963}\ used %'=33%;79;89;0;100
```

To check the size of **a mounted volume** (c:\volume_test) and **make sure it has 1M free** space **warn if free space is less then 10M**::

```
   check_drivesize "crit=free<1M" "warn=free<10M" drive=c:\\volume_test
   C:: Total: 74.5G - Used: 71.2G (95%) - Free: 3.28G (5%) < critical,C:;5%;10;5;
```

To check the size of **all volumes** and **make sure they have 1M space free**::

```
check_drivesize "crit=free<1M" drive=all-volumes
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'E:\ free'=0B;0;0;0;0 'F:\ free'=0B;0;0;0;0
```

To check the size of **all fixed and network drives** and make sure they have at least 1gig free space::

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')"
L     client OK: All drives ok
L     client  Performance data: 'C:\ free'=18GB;0;2;0;223 'C:\ free %'=8%;0;0;0;100 'D:\ free'=18GB;0;4;0;465 'D:\ free %'=3%;0;0;0;100 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```


To check **all fixed and network drives but ignore C and F**::

```
check_drivesize "crit=free<1g" drive=* "filter=type in ('fixed', 'remote')" exclude=C:\\ exclude=D:\\
L     client OK: All drives ok
L     client  Performance data: 'M:\ free'=83GB;0;27;0;2746 'M:\ free %'=3%;0;0;0;100
```

Default via NRPE:

```
check_nrpe --host 192.168.56.103 --command check_drivesize
C:\: 205GB/223GB used, D:\: 448GB/466GB used, M:\: 2.6TB/2.68TB used|'C:\ used'=204GB;44;22;0;223 'C:\ used %'=91%;19;9;0;100 'D:\ used'=447GB;93;46;0;465...
```



<a name="check_drivesize_warn"/>
<a name="check_drivesize_crit"/>
<a name="check_drivesize_debug"/>
<a name="check_drivesize_show-all"/>
<a name="check_drivesize_escape-html"/>
<a name="check_drivesize_help"/>
<a name="check_drivesize_help-pb"/>
<a name="check_drivesize_show-default"/>
<a name="check_drivesize_help-short"/>
<a name="check_drivesize_mounted"/>
<a name="check_drivesize_magic"/>
<a name="check_drivesize_exclude"/>
<a name="check_drivesize_total"/>
<a name="check_drivesize_options"/>
#### Command-line Arguments


| Option                                                  | Default Value                          | Description                                                                                                      |
|---------------------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_drivesize_filter)                       | mounted = 1                            | Filter which marks interesting items.                                                                            |
| [warning](#check_drivesize_warning)                     | used > 80%                             | Filter which marks items which generates a warning state.                                                        |
| warn                                                    |                                        | Short alias for warning                                                                                          |
| [critical](#check_drivesize_critical)                   | used > 90%                             | Filter which marks items which generates a critical state.                                                       |
| crit                                                    |                                        | Short alias for critical.                                                                                        |
| [ok](#check_drivesize_ok)                               |                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                                   | N/A                                    | Show debugging information in the log                                                                            |
| show-all                                                | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_drivesize_empty-state)             | unknown                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_drivesize_perf-config)             |                                        | Performance data generation configuration                                                                        |
| escape-html                                             | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                                    | N/A                                    | Show help screen (this screen)                                                                                   |
| help-pb                                                 | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                                            | N/A                                    | Show default values for a given command                                                                          |
| help-short                                              | N/A                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_drivesize_top-syntax)               | ${status} ${problem_list}              | Top level syntax.                                                                                                |
| [ok-syntax](#check_drivesize_ok-syntax)                 | %(status) All %(count) drive(s) are ok | ok syntax.                                                                                                       |
| [empty-syntax](#check_drivesize_empty-syntax)           | %(status): No drives found             | Empty syntax.                                                                                                    |
| [detail-syntax](#check_drivesize_detail-syntax)         | ${drive_or_name}: ${used}/${size} used | Detail level syntax.                                                                                             |
| [perf-syntax](#check_drivesize_perf-syntax)             | ${drive_or_id}                         | Performance alias syntax.                                                                                        |
| [drive](#check_drivesize_drive)                         |                                        | The drives to check.                                                                                             |
| [ignore-unreadable](#check_drivesize_ignore-unreadable) | N/A                                    | DEPRECATED (manually set filter instead) Ignore drives which are not reachable by the current user.              |
| mounted                                                 | N/A                                    | DEPRECATED (this is now default) Show only mounted rives i.e. drives which have a mount point.                   |
| magic                                                   |                                        | Magic number for use with scaling drive sizes.                                                                   |
| exclude                                                 |                                        | A list of drives not to check                                                                                    |
| total                                                   | N/A                                    | Include the total of all matching drives                                                                         |



<h5 id="check_drivesize_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `mounted = 1`

<h5 id="check_drivesize_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `used > 80%`

<h5 id="check_drivesize_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `used > 90%`

<h5 id="check_drivesize_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_drivesize_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_drivesize_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_drivesize_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status} ${problem_list}`

<h5 id="check_drivesize_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status) All %(count) drive(s) are ok`

<h5 id="check_drivesize_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No drives found`

<h5 id="check_drivesize_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${drive_or_name}: ${used}/${size} used`

<h5 id="check_drivesize_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${drive_or_id}`

<h5 id="check_drivesize_drive">drive:</h5>

The drives to check.
Multiple options can be used to check more then one drive or wildcards can be used to indicate multiple drives to check. Examples: drive=c, drive=d:, drive=*, drive=all-volumes, drive=all-drives


<h5 id="check_drivesize_ignore-unreadable">ignore-unreadable:</h5>

DEPRECATED (manually set filter instead) Ignore drives which are not reachable by the current user.
For instance Microsoft Office creates a drive which cannot be read by normal users.



<a name="check_drivesize_filter_keys"/>
#### Filter keywords


| Option         | Description                                                                                                  |
|----------------|--------------------------------------------------------------------------------------------------------------|
| count          | Number of items matching the filter. Common option for all checks.                                           |
| crit_count     | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list      | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list    | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| drive          | Technical name of drive                                                                                      |
| drive_or_id    | Drive letter if present if not use id                                                                        |
| drive_or_name  | Drive letter if present if not use name                                                                      |
| erasable       | 1 (true) if drive is erasable                                                                                |
| flags          | String representation of flags                                                                               |
| free           | Shorthand for total_free (Number of free bytes)                                                              |
| free_pct       | Shorthand for total_free_pct (% free space)                                                                  |
| hotplug        | 1 (true) if drive is hotplugable                                                                             |
| id             | Drive or id of drive                                                                                         |
| letter         | Letter the drive is mountedd on                                                                              |
| list           | A list of all items which matched the filter. Common option for all checks.                                  |
| media_type     | Get the media type                                                                                           |
| mounted        | Check if a drive is mounted                                                                                  |
| name           | Descriptive name of drive                                                                                    |
| ok_count       | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list        | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count  | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| readable       | 1 (true) if drive is readable                                                                                |
| removable      | 1 (true) if drive is removable                                                                               |
| size           | Total size of drive                                                                                          |
| status         | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total          | Total number of items. Common option for all checks.                                                         |
| total_free     | Number of free bytes                                                                                         |
| total_free_pct | % free space                                                                                                 |
| total_used     | Number of used bytes                                                                                         |
| total_used_pct | % used space                                                                                                 |
| type           | Type of drive                                                                                                |
| used           | Number of used bytes                                                                                         |
| used_pct       | Shorthand for total_used_pct (% used space)                                                                  |
| user_free      | Free space available to user (which runs NSClient++)                                                         |
| user_free_pct  | % free space available to user                                                                               |
| user_used      | Number of used bytes (related to user)                                                                       |
| user_used_pct  | % used space available to user                                                                               |
| warn_count     | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list      | A list of all items which matched the warning criteria. Common option for all checks.                        |
| writable       | 1 (true) if drive is writable                                                                                |


### check_files

Check various aspects of a file and/or folder.

* [Samples](#check_files_samples)

* [Command-line Arguments](#check_files_options)
* [Filter keywords](#check_files_filter_keys)


<a name="check_files_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckDisk_check_files_samples.md)_

#### Performance

Order is somewhat important but mainly in the fact that some operations are more costly then others.
For instance line_count requires us to read and count the lines in each file so choosing between the following:
Fast version: `filter=creation < -2d and line_count > 100`

Slow version: `filter=line_count > 100 and creation < -2d`

The first one will be significantly faster if you have a thousand old files and 3 new ones.

On the other hand in this example `filter=creation < -2d and size > 100k` swapping them would not be noticeable.

#### Checking versions of .exe files

```
check_files path=c:/foo/ pattern=*.exe "filter=version != '1.0'" "detail-syntax=%(filename): %(version)" "warn=count > 1" show-all
L        cli WARNING: WARNING: 0/11 files (check_nrpe.exe: , nscp.exe: 0.5.0.16, reporter.exe: 0.5.0.16)
L        cli  Performance data: 'count'=11;1;0
```

#### Using the line count with limited recursion:

```
check_files path=c:/windows pattern=*.txt max-depth=1 "filter=line_count gt 100" "detail-syntax=%(filename): %(line_count)" "warn=count>0" show-all
L        cli WARNING: WARNING: 0/1 files (AsChkDev.txt: 328)
L        cli  Performance data: 'count'=1;0;0
```

#### Check file sizes

```
check_files path=c:/windows pattern=*.txt "detail-syntax=%(filename): %(size)" "warn=size>20k" max-depth=1
L        cli WARNING: WARNING: 1/6 files (AsChkDev.txt: 29738)
L        cli  Performance data: 'AsChkDev.txt size'=29.04101KB;20;0 'AsDCDVer.txt size'=0.02246KB;20;0 'AsHDIVer.txt size'=0.02734KB;20;0 'AsPEToolVer.txt size'=0.08789KB;20;0 'AsToolCDVer.txt size'=0.05273KB;20;0 'csup.txt size'=0.00976KB;20;0
```



<a name="check_files_warn"/>
<a name="check_files_crit"/>
<a name="check_files_debug"/>
<a name="check_files_show-all"/>
<a name="check_files_escape-html"/>
<a name="check_files_help"/>
<a name="check_files_help-pb"/>
<a name="check_files_show-default"/>
<a name="check_files_help-short"/>
<a name="check_files_file"/>
<a name="check_files_paths"/>
<a name="check_files_max-depth"/>
<a name="check_files_options"/>
#### Command-line Arguments


| Option                                      | Default Value                                                | Description                                                                                                      |
|---------------------------------------------|--------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_files_filter)               |                                                              | Filter which marks interesting items.                                                                            |
| [warning](#check_files_warning)             |                                                              | Filter which marks items which generates a warning state.                                                        |
| warn                                        |                                                              | Short alias for warning                                                                                          |
| [critical](#check_files_critical)           |                                                              | Filter which marks items which generates a critical state.                                                       |
| crit                                        |                                                              | Short alias for critical.                                                                                        |
| [ok](#check_files_ok)                       |                                                              | Filter which marks items which generates an ok state.                                                            |
| debug                                       | N/A                                                          | Show debugging information in the log                                                                            |
| show-all                                    | N/A                                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_files_empty-state)     | unknown                                                      | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_files_perf-config)     |                                                              | Performance data generation configuration                                                                        |
| escape-html                                 | N/A                                                          | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                        | N/A                                                          | Show help screen (this screen)                                                                                   |
| help-pb                                     | N/A                                                          | Show help screen as a protocol buffer payload                                                                    |
| show-default                                | N/A                                                          | Show default values for a given command                                                                          |
| help-short                                  | N/A                                                          | Show help screen (short format).                                                                                 |
| [top-syntax](#check_files_top-syntax)       | ${status}: ${problem_count}/${count} files (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_files_ok-syntax)         | %(status): All %(count) files are ok                         | ok syntax.                                                                                                       |
| [empty-syntax](#check_files_empty-syntax)   | No files found                                               | Empty syntax.                                                                                                    |
| [detail-syntax](#check_files_detail-syntax) | ${name}                                                      | Detail level syntax.                                                                                             |
| [perf-syntax](#check_files_perf-syntax)     | ${name}                                                      | Performance alias syntax.                                                                                        |
| [path](#check_files_path)                   |                                                              | The path to search for files under.                                                                              |
| file                                        |                                                              | Alias for path.                                                                                                  |
| paths                                       |                                                              | A comma separated list of paths to scan                                                                          |
| [pattern](#check_files_pattern)             | *.*                                                          | The pattern of files to search for (works like a filter but is faster and can be combined with a filter).        |
| max-depth                                   |                                                              | Maximum depth to recurse                                                                                         |
| [total](#check_files_total)                 | filter                                                       | Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter   |



<h5 id="check_files_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_files_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_files_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_files_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_files_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_files_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_files_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${problem_count}/${count} files (${problem_list})`

<h5 id="check_files_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All %(count) files are ok`

<h5 id="check_files_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No files found`

<h5 id="check_files_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${name}`

<h5 id="check_files_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`

<h5 id="check_files_path">path:</h5>

The path to search for files under.
Notice that specifying multiple path will create an aggregate set you will not check each path individually.In other words if one path contains an error the entire check will result in error.


<h5 id="check_files_pattern">pattern:</h5>

The pattern of files to search for (works like a filter but is faster and can be combined with a filter).

*Default Value:* `*.*`

<h5 id="check_files_total">total:</h5>

Include the total of either (filter) all files matching the filter or (all) all files regardless of the filter

*Default Value:* `filter`


<a name="check_files_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| access        | Last access time                                                                                             |
| access_l      | Last access time (local time)                                                                                |
| access_u      | Last access time (UTC)                                                                                       |
| age           | Seconds since file was last written                                                                          |
| count         | Number of items matching the filter. Common option for all checks.                                           |
| creation      | When file was created                                                                                        |
| creation_l    | When file was created (local time)                                                                           |
| creation_u    | When file was created (UTC)                                                                                  |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| file          | The name of the file                                                                                         |
| filename      | The name of the file                                                                                         |
| line_count    | Number of lines in the file (text files)                                                                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| name          | The name of the file                                                                                         |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| path          | Path of file                                                                                                 |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| size          | File size                                                                                                    |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| type          | Type of item (file or dir)                                                                                   |
| version       | Windows exe/dll file version                                                                                 |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |
| write         | Alias for written                                                                                            |
| written       | When file was last written to                                                                                |
| written_l     | When file was last written  to (local time)                                                                  |
| written_u     | When file was last written  to (UTC)                                                                         |


### checkdrivesize

Legacy version of check_drivesize


* [Command-line Arguments](#checkdrivesize_options)





<a name="checkdrivesize_help"/>
<a name="checkdrivesize_help-pb"/>
<a name="checkdrivesize_show-default"/>
<a name="checkdrivesize_help-short"/>
<a name="checkdrivesize_Drive"/>
<a name="checkdrivesize_FilterType"/>
<a name="checkdrivesize_perf-unit"/>
<a name="checkdrivesize_MaxWarn"/>
<a name="checkdrivesize_MaxCrit"/>
<a name="checkdrivesize_MinWarn"/>
<a name="checkdrivesize_MinCrit"/>
<a name="checkdrivesize_MaxWarnFree"/>
<a name="checkdrivesize_MaxCritFree"/>
<a name="checkdrivesize_MinWarnFree"/>
<a name="checkdrivesize_MinCritFree"/>
<a name="checkdrivesize_MaxWarnUsed"/>
<a name="checkdrivesize_MaxCritUsed"/>
<a name="checkdrivesize_MinWarnUsed"/>
<a name="checkdrivesize_MinCritUsed"/>
<a name="checkdrivesize_options"/>
#### Command-line Arguments


| Option                                           | Default Value | Description                                                                                                                |
|--------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------|
| help                                             | N/A           | Show help screen (this screen)                                                                                             |
| help-pb                                          | N/A           | Show help screen as a protocol buffer payload                                                                              |
| show-default                                     | N/A           | Show default values for a given command                                                                                    |
| help-short                                       | N/A           | Show help screen (short format).                                                                                           |
| [CheckAll](#checkdrivesize_CheckAll)             | true          | Checks all drives.                                                                                                         |
| [CheckAllOthers](#checkdrivesize_CheckAllOthers) | true          | Checks all drives turns the drive option into an exclude option.                                                           |
| Drive                                            |               | The drives to check                                                                                                        |
| FilterType                                       |               | The type of drives to check fixed, remote, cdrom, ramdisk, removable                                                       |
| perf-unit                                        |               | Force performance data to use a given unit prevents scaling which can cause problems over time in some graphing solutions. |
| [ShowAll](#checkdrivesize_ShowAll)               | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores).                      |
| MaxWarn                                          |               | Maximum value before a warning is returned.                                                                                |
| MaxCrit                                          |               | Maximum value before a critical is returned.                                                                               |
| MinWarn                                          |               | Minimum value before a warning is returned.                                                                                |
| MinCrit                                          |               | Minimum value before a critical is returned.                                                                               |
| MaxWarnFree                                      |               | Maximum value before a warning is returned.                                                                                |
| MaxCritFree                                      |               | Maximum value before a critical is returned.                                                                               |
| MinWarnFree                                      |               | Minimum value before a warning is returned.                                                                                |
| MinCritFree                                      |               | Minimum value before a critical is returned.                                                                               |
| MaxWarnUsed                                      |               | Maximum value before a warning is returned.                                                                                |
| MaxCritUsed                                      |               | Maximum value before a critical is returned.                                                                               |
| MinWarnUsed                                      |               | Minimum value before a warning is returned.                                                                                |
| MinCritUsed                                      |               | Minimum value before a critical is returned.                                                                               |



<h5 id="checkdrivesize_CheckAll">CheckAll:</h5>

Checks all drives.

*Default Value:* `true`

<h5 id="checkdrivesize_CheckAllOthers">CheckAllOthers:</h5>

Checks all drives turns the drive option into an exclude option.

*Default Value:* `true`

<h5 id="checkdrivesize_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkfiles

Legacy version of check_drivesize


* [Command-line Arguments](#checkfiles_options)





<a name="checkfiles_help"/>
<a name="checkfiles_help-pb"/>
<a name="checkfiles_show-default"/>
<a name="checkfiles_help-short"/>
<a name="checkfiles_syntax"/>
<a name="checkfiles_master-syntax"/>
<a name="checkfiles_path"/>
<a name="checkfiles_pattern"/>
<a name="checkfiles_alias"/>
<a name="checkfiles_debug"/>
<a name="checkfiles_max-dir-depth"/>
<a name="checkfiles_filter"/>
<a name="checkfiles_warn"/>
<a name="checkfiles_crit"/>
<a name="checkfiles_MaxWarn"/>
<a name="checkfiles_MaxCrit"/>
<a name="checkfiles_MinWarn"/>
<a name="checkfiles_MinCrit"/>
<a name="checkfiles_options"/>
#### Command-line Arguments


| Option        | Default Value | Description                                         |
|---------------|---------------|-----------------------------------------------------|
| help          | N/A           | Show help screen (this screen)                      |
| help-pb       | N/A           | Show help screen as a protocol buffer payload       |
| show-default  | N/A           | Show default values for a given command             |
| help-short    | N/A           | Show help screen (short format).                    |
| syntax        |               | Syntax for individual items (detail-syntax).        |
| master-syntax |               | Syntax for top syntax (top-syntax).                 |
| path          |               | The file or path to check                           |
| pattern       |               | Deprecated and ignored                              |
| alias         |               | Deprecated and ignored                              |
| debug         | N/A           | Debug                                               |
| max-dir-depth |               | The maximum level to recurse                        |
| filter        |               | The filter to use when including files in the check |
| warn          |               | Deprecated and ignored                              |
| crit          |               | Deprecated and ignored                              |
| MaxWarn       |               | Maximum value before a warning is returned.         |
| MaxCrit       |               | Maximum value before a critical is returned.        |
| MinWarn       |               | Minimum value before a warning is returned.         |
| MinCrit       |               | Minimum value before a critical is returned.        |






