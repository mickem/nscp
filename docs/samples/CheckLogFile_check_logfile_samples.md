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

**Report a quiet check as something other than OK**

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark=app-errors empty-state=unknown
UNKNOWN: Nothing found|'count'=0;0;0
```
