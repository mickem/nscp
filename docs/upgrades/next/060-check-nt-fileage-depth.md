---
icon: "🔒"
modules: [NSClientServer, CheckDisk]
action: conditional
---
**`check_nt` `FILEAGE` now checks one file instead of a directory of them.** The
legacy request mapped onto `check_files` with unlimited recursion, so a
directory argument answered with every file beneath it and its modification
time. Worse, `FILEAGE` returns a single age taken from the first performance
value, so a directory argument reported whichever file the walk happened to
emit first rather than the oldest or the newest.

The request now maps onto `check_single_file`, which stats exactly one path. A
`FILEAGE` naming a single file, which is what the original check did, is
unaffected and reports the same age as before. A `FILEAGE` naming a directory
now fails with an error instead of returning an arbitrary file's age, and one
naming a missing file reports that it was not found. If any check depends on
passing a directory, point it at the file it actually means.

`check_single_file` lives in `CheckDisk`, as `check_files` did, so no extra
module needs enabling.
See the [security notice](../security/notices.md#script-execution-and-check-arguments-nul-truncation-import-sandbox-pipe-reads-handle-leak-docker-endpoint-remote-connection-checks).
