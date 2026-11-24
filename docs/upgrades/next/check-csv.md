---
icon: "📄"
modules: [CheckLogFile]
action: none
---
**New check: `check_csv` (CheckLogFile).** Reads a local CSV file (or, on
Windows, one on a UNC share) and runs the filter over its rows. Each column
declared in `columns=` (an SQL-style list such as `'Hostname TEXT,
WaitingUpdates INTEGER, Load REAL'`) becomes a keyword of that name and type, or
`header=true` takes the names from the header row. That covers both usual modes
without an SQL engine: threshold how many rows match (`filter=...
critical=count > 0`), or threshold one cell of the row `filter=` selects. The
parser is RFC 4180 (quoted fields, doubled quotes, line breaks inside quotes,
backslashes kept as written) and takes `delimiter=`, `quote=` and
`skip-lines=`; `encoding=auto` reads the UTF-16 files Windows exports often
are. It reads file contents just as `check_logfile` does, so it shares
`check_logfile`'s `file access` gate in `[/settings/logfile]`: a host that has
restricted that setting has restricted `check_csv` too. The check is marked
experimental.
