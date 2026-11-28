---
icon: "🔧"
modules: [CheckDisk, CheckHyperV, CheckTaskSched, CommandClient, SimpleCache, SimpleFileWriter]
action: none
---
**Web UI: pages for the disk checks and the remaining small modules.**
Nothing to do.

- **Disks and files** (CheckDisk): which paths `check_files`,
  `check_single_file`, `check_largest_files` and `check_disk_write` may be
  given - any path (flagged), paths under an allow list, or only the named
  paths - with the named paths below, each with a *Run* button that runs
  `check_single_file` by name; the growth-spots diagnostics, the
  `storage.volumes` fact set, and under *Advanced* the collector's intervals
  and the collectors switched off.
- **Hyper-V** and **Scheduled tasks**: their fact sets.
- **Command line** (CommandClient): the `nscp test` prompt's history and
  colour.
- **Result cache** (SimpleCache) and **File writer** (SimpleFileWriter): the
  channel they listen on, the cache key, the file and the line written.

Every module in the reference with settings now has a page of its own; expert mode still shows
every key. A module that registers no settings (CheckNet, CheckSecurity, ...)
says so instead of showing an empty form.

On every tailored module page, the lists (named paths, queries, targets,
schedules, scripts, aliases, roles, ...) no longer write a row the moment its
dialog closes: the row is queued, shown in the list and in the save bar, and
written with the page's own fields by **Save** (or dropped by **Discard**).
Storing what the agent holds in its configuration file and reloading it is a
separate step in the same bar - **Save and reload**, next to **Show diff** -
shown while the agent has changes its file lacks. Expert mode still writes
each key as before.
