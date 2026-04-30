#### About `check_single_file`

`check_single_file` is a focused variant of [`check_files`](CheckDisk_check_files_samples.md)
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

