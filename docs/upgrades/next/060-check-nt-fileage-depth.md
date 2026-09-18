---
modules: [NSClientServer]
action: conditional
---
**`check_nt` `FILEAGE` no longer walks a whole directory tree.** The legacy
request maps onto `check_files`, and did so with unlimited recursion: a
directory argument answered with every file beneath it and its modification
time. The mapping now passes `max-depth=0`, so a directory argument reports the
files directly inside it and nothing deeper. A `FILEAGE` naming a single file,
which is what the original check did, is unaffected.
