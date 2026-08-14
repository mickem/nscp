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
`bookmark` with no value (or `bookmark=auto`), which builds it from the file
name plus the `filter`, `warning` and `critical` expressions:

```
check_logfile file=/var/log/app.log "filter=column1 like 'ERROR'" "warning=count > 0" bookmark
```

Positions are kept per file, so `file=` may be repeated as usual; each file is
tracked on its own. They are written to `${data-path}/nsclient.db` when
NSClient++ shuts down and restored on start, so a restart does not re-report
everything.

Because an automatic name embeds the expressions, editing the filter starts a
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
* **A quiet check is OK, not UNKNOWN.** When nothing new arrived the result is
  the empty state (`%(status): Nothing found`), which is OK by default; use
  `empty-state=` to change it.
* **Checks without `bookmark` are unaffected.** They neither read nor advance
  any stored position, so an ad-hoc full scan can be run at any time without
  disturbing a bookmarked check.

Real-time monitoring (`/settings/logfile/real-time/checks`) is the other way to
get each line reported once; it pushes results as lines are written instead of
being polled. `bookmark` is the polled equivalent and needs no configuration on
the agent.
