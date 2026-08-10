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
OK: 27 processes owned by root
L        cli  Performance data: 'count'=27;1000;0 ...
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
bash up 961756s, bash up 961732s, bash up 6183s, bash up 6s
L        cli  Performance data: 'bash elapsed'=961756s;0;31536000 ...
```

**`rss` is an alias for `working_set` (same value, portable with the Windows
check):**

```
check_process process=bash "crit=rss > 2G" "top-syntax=${list}" "detail-syntax=${exe} rss=${rss} ws=${working_set}"
bash rss=8.594MB ws=8.594MB, bash rss=9.219MB ws=9.219MB, bash rss=4.688MB ws=4.688MB
L        cli  Performance data: 'bash rss'=0.00839GB;0;2 ...
```
