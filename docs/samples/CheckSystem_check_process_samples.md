**Default check:**

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
WARNING: Explorer.EXE ws:431.812MB, handles: 5639, user time:2535s
Performance data: 'explorer.exe ws_size'=73M;70;0
```

List all processes which use **more then 200m virtual memory** Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process --arguments "filter=virtual > 200m"
OK all processes are ok.|'csrss.exe state'=1;0;0 'svchost.exe state'=1;0;0 'AvastSvc.exe state'=1;0;0 ...
```

**Thread count**::

```
check_process process=chrome.exe "warn=thread_count > 400" "detail-syntax=${exe}: ${thread_count} threads"
OK: chrome.exe: 212 threads
Performance data: 'chrome.exe threads'=212;400;0
```

**Percentage-of-RAM / percentage-of-commit** thresholds::

```
check_process process=sqlservr.exe "warn=working_set_pct > 25" "crit=working_set_pct > 40" "detail-syntax=${exe}: ${working_set_pct}% RAM, ${pagefile_pct}% commit"
OK: sqlservr.exe: 12% RAM, 8% commit
Performance data: 'sqlservr.exe ws_pct'=12%;25;40 'sqlservr.exe pf_pct'=8%;;
```

`working_set_pct` is the process working set as a percentage of total physical
RAM; `pagefile_pct` is its pagefile (commit) usage as a percentage of the system
commit limit (RAM + pagefile). Both work with `total=true` aggregation.

