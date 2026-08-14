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
