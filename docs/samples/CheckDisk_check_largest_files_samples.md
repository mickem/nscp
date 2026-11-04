**The five largest files under a folder**

```
check_largest_files path=C:\Windows top=5
OK: C:\Windows\Installer\638c1ee.msi (639.852MB), C:\Windows\Installer\5f92.msi (376.047MB), C:\Windows\System32\Microsoft-Edge-WebView\msedge.dll (327.746MB), C:\Windows\WinSxS\amd64_microsoft-edge-webview_31bf3856ad364e35_10.0.26100.9711_none_2e9083253adbaceb\msedge.dll (327.746MB), C:\Windows\WinSxS\amd64_microsoft-edge-webview_31bf3856ad364e35_10.0.26100.9444_none_2eb3af6f3ac0f200\msedge.dll (325.891MB)
```

**The largest folders directly under a folder**

```
check_largest_files path=C:\Windows type=folders top=5
OK: C:\Windows\WinSxS (17.753GB), C:\Windows\System32 (10.008GB), C:\Windows\Installer (5.893GB), C:\Windows\assembly (3.502GB), C:\Windows\SystemApps (1.36GB)
```

This was a walk, which counts a hard-linked file once per link (see
*How it scans* above).

**Alert on large log files**

```
check_largest_files path=C:\Windows top=3 pattern=*.log "warning=size > 10M" "critical=size > 100M"
CRITICAL: C:\Windows\Logs\CBS\CbsPersist_20261020173512.log (320.883MB), C:\Windows\Logs\CBS\CbsPersist_20261025144928.log (16.392MB), C:\Windows\debug\WIA\wiatrace.bak.log (16MB)|'C:\Windows\Logs\CBS\CbsPersist_20261020173512.log'=320.88317MB;10;100 'C:\Windows\Logs\CBS\CbsPersist_20261025144928.log'=16.39163MB;10;100 'C:\Windows\debug\WIA\wiatrace.bak.log'=16.00015MB;10;100
```

**Warn when a folder grows (Linux)**

```
check_largest_files path=/usr type=folders top=3 "warning=size > 1G" "critical=size > 5G"
WARNING: /usr/lib (1.258GB), /usr/share (403.404MB), /usr/bin (222.715MB)|'/usr/lib'=1.25844GB;1;5 '/usr/share'=0.39394GB;1;5 '/usr/bin'=0.21749GB;1;5
```

**Rank folders at every depth, with their file counts**

```
check_largest_files path=C:\Windows\System32 type=folders folder-depth=-1 top=3 "detail-syntax=${path}: ${size} in ${file_count} files"
OK: C:\Windows\System32\DriverStore: 4.945GB in 10816 files, C:\Windows\System32\DriverStore\FileRepository: 4.942GB in 9856 files, C:\Windows\System32\DriverStore\FileRepository\nv_dispig.inf_amd64_cc526ccc3287f5cd: 2.675GB in 360 files
```

**A compact, ranked list**

```
check_largest_files path=C:\Windows top=3 "detail-syntax=${rank}. ${name} ${size}"
OK: 1. 638c1ee.msi 639.852MB, 2. 5f92.msi 376.047MB, 3. msedge.dll 327.746MB
```

**Nothing above the minimum size**

```
check_largest_files path=/var/log top=3 pattern=*.log min-size=1M
OK: No files found
```

**Insisting on the Master File Table without the rights to read it**

```
check_largest_files path=C:\Windows method=mft
Unable to read the MFT for C:\Windows: unable to open the volume for reading (this needs administrator rights): 5: Access is denied.
```
