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
