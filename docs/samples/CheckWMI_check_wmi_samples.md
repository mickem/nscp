Basic check to see/fetch information (no check)::

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

