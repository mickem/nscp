.. default-domain:: nscp

.. default-domain:: nscp

===========
All samples
===========

A collection of all sample commands


CauseCrashes
============

crash_client
------------

Raise a fatal exception (zero pointer reference) and cause NSClient++ crash.

.. include:: ../samples/Configuration to setup the module::

```
[/modules]
NRPEServer = enabled
CauseCrashes = enabled

[/settings/NRPE/server]
allowed hosts = 127.0.0.1
```

Then execute the following command on Nagios::

```
nscp nrpe --host 127.0.0.1 --command crashclient
```

Then execute the following command on the NSClient++ machine::

```
nscp test
...
crashclient
```

This will cause NSClient++ to crash so please dont do this.


CheckDisk
=========

check_drivesize
---------------

Check the size (free-space) of a drive or volume.

.. include:: ../samples/To check the size of **the C:\ drive** and **make sure it has atleast 10% free** space::

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


check_files
-----------

Check various aspects of a file and/or folder.

.. include:: ../samples/#### Performance

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

### Check file sizes

```
check_files path=c:/windows pattern=*.txt "detail-syntax=%(filename): %(size)" "warn=size>20k" max-depth=1
L        cli WARNING: WARNING: 1/6 files (AsChkDev.txt: 29738)
L        cli  Performance data: 'AsChkDev.txt size'=29.04101KB;20;0 'AsDCDVer.txt size'=0.02246KB;20;0 'AsHDIVer.txt size'=0.02734KB;20;0 'AsPEToolVer.txt size'=0.08789KB;20;0 'AsToolCDVer.txt size'=0.05273KB;20;0 'csup.txt size'=0.00976KB;20;0
```











CheckSystem
===========

check_cpu
---------

Check that the load of the CPU(s) are within bounds.

.. include:: ../samples/**Default check:**

```
check_cpu
CPU Load ok
'total 5m load'=0%;80;90 'total 1m load'=0%;80;90 'total 5s load'=7%;80;90
```

Checking **all cores** by adding filter=none (disabling the filter):

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
CPU Load ok
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

Adding kernel times to the check::

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90" "top-syntax=${list}"
core 0 > 3, core 1 > 0, core 2 > 0, core  ... , core 7 > 15, total > 7
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

Default check **via NRPE**::

```
check_nscp --host 192.168.56.103 --command check_cpu
CPU Load ok|'total 5m'=16%;80;90 'total 1m'=13%;80;90 'total 5s'=13%;80;90
```


check_memory
------------

Check free/used memory on the system.

.. include:: ../samples/**Default check:**

```
check_memory
OK memory within bounds.
'page used'=8G;19;21 'page used %'=33%;79;89 'physical used'=7G;9;10 'physical used %'=65%;79;89
```

Using --show-all **to show the result**:


```
check_memory "warn=free < 20%" "crit=free < 10G" --show-all
page = 8.05G, physical = 7.85G
'page free'=15G;4;2 'page free %'=66%;19;9 'physical free'=4G;2;1 'physical free %'=34%;19;9
```

Changing the return syntax to include more information::

```
check_memory "top-syntax=${list}" "detail-syntax=${type} free: ${free} used: ${used} size: ${size}"
page free: 16G used: 7.98G size: 24G, physical free: 4.18G used: 7.8G size: 12G
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_memory
OK memory within bounds.|'page'=531G;3;3;0;3 'page %'=12%;79;89;0;100 'physical'=530G;1;1;0;1 'physical %'=25%;79;89;0;100
```


check_os_version
----------------

Check the version of the underlaying OS.

.. include:: ../samples/**Default check:**

```
check_os_Version
L     client CRITICAL: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;50;50
```

Making sure the OS version is **Windows 8**:

```
check_os_Version "warn=version < 62"
L     client WARNING: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;62;0
```

Default check **via NRPE**:

```
check_nrpe --host 192.168.56.103 --command check_os_version
Windows 2012 (6.2.9200)|'version'=62;50;50
```



check_pagefile
--------------

Check the size of the system pagefile(s).

.. include:: ../samples/**Default options:**

```
check_pagefile
L     client WARNING: \Device\HarddiskVolume2\pagefile.sys 24.3M (32M)
L     client  Performance data: '\??\D:\pagefile.sys'=1G;14;19;0;23 '\??\D:\pagefile.sys %'=6%;59;79;0;100 '\Device\HarddiskVolume2\pagefile.sys'=24M;19;25;0;32 '\Device\HarddiskVolume2\pagefile.sys %'=75%;59;79;0;100 'total'=1G;14;19;0;23 'total %'=6%;59;79;0;100
```

Only showing the total amount of pagefile usage::

```
check_pagefile "filter=name = 'total'" "top-syntax=${list}"
OK: total 1.66G (24G)
Performance data: 'total'=1G;14;19;0;23 'total %'=6%;59;79;0;100

```

Getting help on avalible options::

```
check_pagefile help
...
  filter=ARG           Filter which marks interesting items.
					   Interesting items are items which will be included in
					   the check.
					   They do not denote warning or critical state but they
					   are checked use this to filter out unwanted items.
						   Avalible options:
					   free          Free memory in bytes (g,m,k,b) or percentages %
					   name          The name of the page file (location)
					   size          Total size of pagefile
					   used          Used memory in bytes (g,m,k,b) or percentages %
					   count         Number of items matching the filter
					   total         Total number of items
					   ok_count      Number of items matched the ok criteria
					   warn_count    Number of items matched the warning criteria
					   crit_count    Number of items matched the critical criteria
					   problem_count Number of items matched either warning or critical criteria
...
```


check_pdh
---------

Check the value of a performance (PDH) counter on the local or remote system.

.. include:: ../samples/﻿**Checking specific Counter (\System\System Up Time):**

```
check_pdh "counter=\\System\\System Up Time" "warn=value > 5" "crit=value > 9999"
\System\System Up Time = 204213
'\System\System Up Time value'=204213;5;9999
```

Using the **expand index** to check for translated counters::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" expand-index
Everything looks good
'\Minne\Dedikationsgräns value'=-2147483648;5;9999
```

Checking **translated counters** without expanding indexes::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999"
Everything looks good
'\4\30 value'=-2147483648;5;9999
```

Checking **large values** using the type=large keyword::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" flags=nocap100 expand-index type=large
\Minne\Dedikationsgräns = 25729224704
'\Minne\Dedikationsgräns value'=25729224704;5;9999
```

Using real-time checks to check avergae values over time.

Here we configure a counter to be checked at regular intervals and the value is added to a rrd buffer.
The configuration from nsclient.ini::

```
[/settings/system/windows/counters/foo]
collection strategy=rrd
type=large
counter=\Processor(_total)\% Processor Time
```

Then we can check the value (**current snapshot**)::

```
check_pdh "counter=foo" "warn=value > 80" "crit=value > 90"
Everything looks good
'foo value'=18;80;90
```

To check averages from the same counter we need to specify the time option::

```
check_pdh "counter=foo" "warn=value > 80" "crit=value > 90" time=30s
Everything looks good
'foo value'=3;80;90
```

Checking **all instances** of a given counter::

```
    check_pdh "counter=\Processor(*)\% processortid" instances
L     client OK: \\MIME-LAPTOP\Processor(0)\% processortid = 100, \\MIME-LAPTOP\Processor(1)\% processortid = 100, \\MIME-LAPTOP\Processor(2)\% processortid = 100, \\MIME-LAPTOP\Processor(3)\% processortid = 100, \\MIME-LAPTOP\Processor(4)\% processortid = 100, \\MIME-LAPTOP\Processor(5)\% processortid = 100, \\MIME-LAPTOP\Processor(6)\% processortid = 100, \\MIME-LAPTOP\Processor(7)\% processortid = 100, \\MIME-LAPTOP\Processor(_Total)\% processortid = 100
    L     client  Performance data: '\Processor(*)\% processortid_0'=100;0;0 '\Processor(*)\% processortid_1'=100;0;0 '\Processor(*)\% processortid_2'=100;0;0 '\Processor(*)\% processortid_3'=100;0;0 '\Processor(*)\% processortid_4'=100;0;0 '\Processor(*)\% processortid_5'=100;0;0 '\Processor(*)\% processortid_6'=100;0;0 '\Processor(*)\% processortid_7'=100;0;0 '\Processor(*)\% processortid__Total'=100;0;0
```


check_process
-------------

Check state/metrics of one or more of the processes running on the computer.

.. include:: ../samples/**Default check:**

```
check_process
SetPoint.exe=hung
Performance data: 'taskhost.exe'=1;1;0 'dwm.exe'=1;1;0 'explorer.exe'=1;1;0 ... 'chrome.exe'=1;1;0 'vcpkgsrv.exe'=1;1;0 'vcpkgsrv.exe'=1;1;0 
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process
SetPoint.exe=hung|'smss.exe state'=1;0;0 'csrss.exe state'=1;0;0...
```

Check that **specific process** are running::

```
check_process process=explorer.exe process=foo.exe
foo.exe=stopped
Performance data: 'explorer.exe'=1;1;0 'foo.exe'=0;1;0
```

Check **memory footprint** from specific processes::

```
check_process process=explorer.exe "warn=working_set > 70m"
explorer.exe=started
Performance data: 'explorer.exe ws_size'=73M;70;0
```

**Extend the syntax** to display the attributes we are interested in::

```
check_process process=explorer.exe "warn=working_set > 70m" "detail-syntax=${exe} ws:${working_set}, handles: ${handles}, user time:${user}s"
explorer.exe ws:77271040, handles: 800, user time:107s
Performance data: 'explorer.exe ws_size'=73M;70;0
```

List all processes which use **more then 200m virtual memory** Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process --arguments "filter=virtual > 200m"
OK all processes are ok.|'csrss.exe state'=1;0;0 'svchost.exe state'=1;0;0 'AvastSvc.exe state'=1;0;0 ...
```



check_service
-------------

Check the state of one or more of the computer services.

.. include:: ../samples/**Default check:**

```
check_service
OK all services are ok.
```

**Excluding services using exclude**::

```
check_service "exclude=clr_optimization_v4.0.30319_32"  "exclude=clr_optimization_v4.0.30319_64"
WARNING: gupdate=stopped (auto), Net Driver HPZ12=stopped (auto), NSClientpp=stopped (auto), nscp=stopped (auto), Pml Driver HPZ12=stopped (auto), SkypeUpdate=stopped (auto), sppsvc=stopped (auto)
```

**Show all service by changing the syntax**::

```
check_service "top-syntax=${list}" "detail-syntax=${name}:${state}"
AdobeActiveFileMonitor10.0:running, AdobeARMservice:running, AdobeFlashPlayerUpdateSvc:stopped, ..., WwanSvc:stopped
```

**Excluding services using the filter**::

```
check_service "filter=start_type = 'auto' and name not in ('Bonjour Service', 'Net Driver HPZ12')"
AdobeActiveFileMonitor10.0: running, AdobeARMservice: running, AMD External Events Utility: running,  ... wuauserv: running
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_service
WARNING: DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)
```

**Check that a service is not started**::

```
check_service service=nscp "crit=state = 'started'" warn=none
```


check_uptime
------------

Check time since last server re-boot.

.. include:: ../samples/**Default check:**

```
check_uptime
uptime: -9:02, boot: 2013-aug-18 08:29:13
'uptime uptime'=1376814553s;1376760683;1376803883
```

Adding **warning and critical thresholds**::

```
check_uptime "warn=uptime < -2d" "crit=uptime < -1d"
...
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_uptime
uptime: -0:3, boot: 2013-sep-08 18:41:06 (UCT)|'uptime'=1378665666;1378579481;1378622681
```




CheckTaskSched
==============

check_tasksched
---------------

Check status of scheduled jobs.

.. include:: ../samples/Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_tasksched
/test: 1 != 0|'test'=1;0;0
```



CheckWMI
========

check_wmi
---------

Check a set of WMI values and return rows which are matching criteria.

.. include:: ../samples/Basic check to see/fetch information (no check)::

```
check_wmi "query=Select Version,Caption from win32_OperatingSystem"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

A simple string check::

```
check_wmi "query=Select Version,Caption from win32_OperatingSystem" "warn=Version not like '6.3'" "crit=Version not like '6'"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

Simple check via **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_wmi -a "query=Select Version,Caption from win32_OperatingSystem" "warn=Version not like '6.3'" "crit=Version not like '6'"
OK: Microsoft Windows 8.1 Pro, 6.3.9600
```

A simple integer (number) check::

```
check_wmi "query=Select BuildNumber from win32_OperatingSystem" "warn=BuildNumber < 9600" "crit=BuildNumber < 8000"
L        cli OK: 9600
L        cli  Performance data: 'BuildNumber'=9600;9600;8000
```

Using performance options to customize the performance data::

```
check_wmi "query=select Name, AvgDiskQueueLength from Win32_PerfFormattedData_PerfDisk_PhysicalDisk" "warn=AvgDiskQueueLength>0" "perf-syntax=%(Name)" "perf-config=*(prefix:'time')"
L        cli OK: 0, _Total, 0, 0 C:, 0, 1 D:
L        cli  Performance data: 'time_Total'=0;0;0 'time0 C:'=0;0;0 'time1 D:'=0;0;0
```

Adding values to the message::

```
check_wmi "query=Select BuildNumber from win32_OperatingSystem" "warn=BuildNumber < 9600" "crit=BuildNumber < 8000" "detail-syntax=You have build %(BuildNumber)" show-all
L        cli OK: You have build 10240
L        cli  Performance data: 'BuildNumber'=10240;9600;8000
```























