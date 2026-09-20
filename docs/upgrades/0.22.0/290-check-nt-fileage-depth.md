---
icon: "💥"
modules: [NSClientServer, CheckDisk]
action: conditional
---
**`check_nt` `FILEAGE` now checks one file instead of a directory of them.**
`FILEAGE` answers with a single age, taken from the first performance value the
underlying check produced. The legacy request mapped onto `check_files`, which
walks a whole tree, so a directory argument reported whichever file the walk
happened to emit first — not the oldest, not the newest, just arbitrary. The
number looked authoritative and meant nothing.

The request now maps onto `check_single_file`, which stats exactly one path, so
there is only ever one candidate and the age is defined. A `FILEAGE` naming a
single file, which is what the check was always for, is unaffected and reports
the same age as before. A `FILEAGE` naming a directory now fails with an error
instead of returning an arbitrary file's age, and one naming a missing file
reports that it was not found. If a check depends on passing a directory, point
it at the file it actually means.

`check_single_file` lives in `CheckDisk`, as `check_files` did, so no extra
module needs enabling.
