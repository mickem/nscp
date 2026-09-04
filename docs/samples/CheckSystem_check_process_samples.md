##### Windows

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

##### Linux

**Show the owner, parent and state of every matching process:**

```
check_process process=bash "top-syntax=${list}" "detail-syntax=${exe} uid=${uid} ppid=${ppid} proc_state=${proc_state} elapsed=${elapsed}s"
bash uid=1000 ppid=385 proc_state=sleeping elapsed=961739s, bash uid=1000 ppid=381 proc_state=sleeping elapsed=961715s, bash uid=0 ppid=12728 proc_state=sleeping elapsed=961693s
```

**Resolve uids to user names (opt-in, `resolve-owner=true`):**

```
check_process process=bash resolve-owner=true "top-syntax=${list}" "detail-syntax=${exe} uid=${uid} owner=${username}"
bash uid=1000 owner=mickem, bash uid=1000 owner=mickem, bash uid=0 owner=root
```

Without the flag `uid` is still populated; only `username` stays empty.

**Count the processes owned by root (`uid` is numeric, so it thresholds
directly):**

```
check_process process=* "filter=uid = 0" "warn=count > 1000" "ok-syntax=%(status): %(count) processes owned by root"
OK: 27 processes owned by root|'count'=27;1000;0 ...
```

**Alert on zombie processes:**

```
check_process process=* "crit=proc_state = 'zombie'" "ok-syntax=%(status): no zombie processes (%(count) checked)" "detail-syntax=%(exe) (pid %(pid)) is a zombie"
OK: no zombie processes (49 checked)
```

**Alert on processes blocked in uninterruptible I/O (a wedged disk or a hung
NFS mount):**

```
check_process process=* "warn=proc_state = 'disk_sleep'" "ok-syntax=%(status): no processes blocked in uninterruptible I/O"
OK: no processes blocked in uninterruptible I/O
```

**Select by parent process id:**

```
check_process process=* "filter=ppid = 1" "ok-syntax=%(status): %(count) processes reparented to init"
OK: 22 processes reparented to init
```

**Threshold on how long a process has been running (`elapsed`, in seconds):**

```
check_process process=bash "crit=elapsed > 31536000" "top-syntax=${list}" "detail-syntax=${exe} up ${elapsed}s"
bash up 961756s, bash up 961732s, bash up 6183s, bash up 6s|'bash elapsed'=961756s;0;31536000 ...
```

**`rss` is an alias for `working_set` (same value, portable with the Windows
check):**

```
check_process process=bash "crit=rss > 2G" "top-syntax=${list}" "detail-syntax=${exe} rss=${rss} ws=${working_set}"
bash rss=8.594MB ws=8.594MB, bash rss=9.219MB ws=9.219MB, bash rss=4.688MB ws=4.688MB|'bash rss'=0.00839GB;0;2 ...
```
