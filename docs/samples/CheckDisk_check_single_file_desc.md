#### About `check_single_file`

`check_single_file` is a focused variant of [`check_files`](#check_files)
for inspecting a single, known path. There is no `path` + `pattern` scan and
no recursion — you point it at one file and apply a filter / threshold to its
attributes (`size`, `age`, `version`, `line_count`, …).

Behaviour at a glance:

* If `file=` (or its alias `path=`) is missing → **UNKNOWN** with
  `No file specified (use file=<path>)`.
* If the file does not exist (or the path points at a directory) →
  **UNKNOWN** with `File not found: <path>`.
* Otherwise the single file is fed to the filter and `warn` / `crit`
  decide the status. With no thresholds the result is **OK** confirming
  the file exists.


#### Files that are legitimately absent (`ignore-missing`)

By default a missing file fails the check, which is what you want when the file
is supposed to be there:

```
check_single_file file=/var/reports/nightly.csv
File not found: /var/reports/nightly.csv
```

Some files are only there some of the time — a lock file, a report written
after a run, a spool entry. `ignore-missing=true` returns OK instead, naming
the path so the result cannot be mistaken for "the file was inspected and was
fine":

```
check_single_file file=/var/reports/nightly.csv ignore-missing=true
File not found (ignored): /var/reports/nightly.csv
```

A file that *is* present is checked exactly as before; the option only affects
the missing case.

The same option exists on [`check_files`](#check_files) (for scan paths) and
[`check_drivesize`](#check_drivesize) (for optional mounts).
