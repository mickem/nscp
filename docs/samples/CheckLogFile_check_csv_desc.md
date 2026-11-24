#### About `check_csv`

`check_csv` reads a CSV file and runs the filter engine over its data rows: each
row is one record, and each column you declare becomes a keyword of that name.
It needs no database engine and no SQL: the two usual ways of checking a CSV are
both expressed with `filter=`, `warning=` and `critical=`:

* **Count matching rows.** Select the rows that are a problem with `filter=`
  and threshold `count` (`critical=count > 0`). With no row matching, the check
  is OK.
* **Threshold one value.** Select a single row with `filter=` and threshold the
  column itself (`warning=WaitingUpdates > 10`). Set `empty-state=unknown` if the
  row being absent is an error.

#### Columns

`columns=` lists the columns in file order, in the shape of an SQL column list:

```
columns=Hostname TEXT, WaitingUpdates INTEGER, Load REAL
```

| Type | Accepted spellings | Behaviour |
|------|--------------------|-----------|
| TEXT | `TEXT`, `VARCHAR(n)`, `CHAR`, `STRING`, or no type | compared as a string |
| INTEGER | `INTEGER`, `INT`, `BIGINT`, `SMALLINT` | compared as a number; an empty or non-numeric cell has no value, renders as `''` and compares false against every number |
| REAL | `REAL`, `FLOAT`, `DOUBLE`, `NUMERIC`, `DECIMAL(p,s)` | compared as a decimal number, also against a whole-number threshold (`Load > 3` is true for 3.75); an empty or non-numeric cell has no value, as for INTEGER, and produces no performance data |

Columns map to fields by position; fields beyond the declared columns are
ignored, and a row with fewer fields has the missing ones empty
(`field_count` tells you how many it had). Without `columns=`, `header=true`
takes the names from the header row, all as TEXT.

A column name must start with a letter and contain only letters, digits and
`_`. It cannot be one of the summary keywords (`count`, `total`, `list`,
`status`, …), the built-in `row`, `field_count` or `values`, or a word of the
filter language (`and`, `like`, …). Names are case-sensitive, so a column
called `Count` or `Status` is fine.

#### Reading the file

* `file=` takes a local path, or on Windows a UNC path
  (`\\server\share\file.csv`), read with the agent's own credentials. On
  Windows the file is opened with every share flag, so an export that is still
  open in the program writing it can be read. There is no network fetch: copy a
  remote CSV to the host, or onto a share, first.
* The format is RFC 4180: a field in quotes may contain the delimiter, line
  breaks and the quote itself written twice (`"say ""hi"""`). A backslash is an
  ordinary character, so Windows paths in cells are read as written. Empty
  lines are skipped.
* `delimiter=` (default `,`; also `tab`, `semicolon`, `pipe`) and `quote=`
  (default `"`, `none` to disable) adapt it to other dialects.
* `skip-lines=` drops lines from the top of the file before it is parsed, such
  as PowerShell's `#TYPE` line or a report title. Every line counts, blank ones
  included, and nothing in a skipped line is interpreted, so a stray quote
  there cannot swallow the rest of the file.
* `encoding=auto` (the default) reads UTF-8, and UTF-16LE or UTF-16BE when the
  file starts with a byte-order mark, as Windows exports often do. Name
  `utf-16le`, `utf-16be`, `utf-8` or `latin1` for a file without one.
* The whole file is read into memory, so it is refused above `max-size`
  (default `16m`). The limit is enforced while reading, not only from the
  size the file reports, and only a regular file is read: a device
  (`/dev/zero`), a FIFO or a Windows named pipe is refused rather than read
  until memory runs out or the other end writes.
* Numbers are read with `.` as the decimal separator (and no thousands
  separator), as CSV exports write them, whatever the host's locale is.

Every problem reading or parsing the file — not found, too large, an
unterminated quote, an unknown column in a filter — is reported as UNKNOWN.

#### Access

`check_csv` returns the contents of the file it reads, exactly as
`check_logfile` does, so the two share one gate: `file access` and `allowed
files` in `[/settings/logfile]`, and the names under
`[/settings/logfile/files]`. See
[Restricting what a check may read](../../concepts/check-access.md).
