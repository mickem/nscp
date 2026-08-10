#### Optional directories (`ignore-missing`)

A top-level `path=` that does not exist fails the check by default, so a
mistyped or unmounted directory is reported rather than silently scanning
nothing:

```
check_files path=/var/spool/exports
Path was not found: /var/spool/exports
```

When a directory is legitimately absent some of the time, `ignore-missing=true`
skips it instead:

```
check_files path=/var/spool/exports ignore-missing=true
No files found
```

Paths that do exist are still scanned, so the missing one simply contributes no
files rather than wiping out the result:

```
check_files path=/var/log path=/var/spool/exports ignore-missing=true pattern=*.log
OK: All 2 files are ok
```

Two details:

* **It implies `empty-state=ok`**, so a scan whose paths are all missing reports
  OK rather than UNKNOWN — otherwise the false CRITICAL is merely traded for a
  false UNKNOWN. Only the default changes; an explicit `empty-state=` wins.
* **The skipped path is not logged as an error.** Without the option a missing
  path writes an `Invalid file specified` error to the log; with it the path is
  expected, so it is logged at debug instead.

The same option exists on [`check_single_file`](#check_single_file) (for the
file itself) and [`check_drivesize`](#check_drivesize) (for optional mounts).
