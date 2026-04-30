#### Confirm a file exists (no thresholds)

```
check_single_file file=C:/Windows/System32/notepad.exe
L        cli OK: notepad.exe (size=201728, age=12345)
```

#### Warn when a log file grows too large

```
check_single_file file=C:/logs/app.log "warn=size > 10M" "crit=size > 100M"
L        cli OK: app.log (size=524288, age=42)
```

#### Warn when a file becomes stale (age in seconds)

```
check_single_file file=C:/windows/WindowsUpdate.log "warn=age > 5m" "crit=age > 1h"
L        cli CRITICAL: WindowsUpdate.log (size=276, age=917)
```

#### Check a specific binary's version

```
check_single_file file="C:/Windows/System32/notepad.exe" "crit=version != '1.2.3.4'" "detail-syntax=%(filename): %(version)"
L        cli CRITICAL: notepad.exe: 6.2.26100.8115
```

#### Custom output formatting

The same `top-syntax` / `detail-syntax` / `ok-syntax` keys as `check_files`
are accepted. Because there is exactly one item, `%(list)` in the top
template expands to the detail line for that single file:

```
check_single_file file=C:/windows/WindowsUpdate.log "warn=size > 1M" "top-syntax=%(status) %(list)" "detail-syntax=%(filename) is %(size) bytes, last written %(written)"
L        cli OK: OK WindowsUpdate.log is 276 bytes, last written 2026-04-30 11:42:36
```

#### `path=` works as an alias for `file=`

This makes it easy to migrate command lines from `check_files`:

```
check_single_file path=C:/Windows/win.ini
L        cli OK: win.ini (size=92, age=873123)
```

