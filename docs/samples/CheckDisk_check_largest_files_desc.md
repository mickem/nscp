#### About `check_largest_files`

`check_largest_files` answers "what is eating this disk?". Point it at a folder
with `path=` and it returns the `top` largest files beneath it, largest first.
With `type=folders` it ranks folders instead, each by the recursive total of
everything beneath it.

* `top` (default 10) is how many entries come back.
* `min-size` (for instance `100M`) leaves out anything smaller. In folder mode
  it applies to the folder totals.
* `pattern` (for instance `*.log`) only counts files whose name matches. In
  folder mode it decides which files count towards the totals.
* `folder-depth` (folder mode, default 1) decides which folders are ranked:
  `1` ranks the direct children of `path`, `N` ranks every folder down to N
  levels below it, and `-1` ranks every folder at any depth. Totals are always
  recursive, so with `-1` a parent always outranks its own children.
* `timeout` (default 30 seconds, `0` for no limit) bounds the scan. A scan that
  runs out of time reports the best of what it saw, appends
  `(scan timed out after Ns, the result is partial)` to the message, and returns
  `timeout-state` (default UNKNOWN). A WARNING or CRITICAL found before the
  timeout still stands, since that file really is that large.

With no thresholds the check is OK and simply lists what it found. Thresholds
apply to each ranked entry, so `warning=size > 10G` warns as soon as any of the
`top` entries is larger than 10 GB. A size threshold also produces perfdata, one
value per entry and named after its path. Sizes are logical file sizes, as a
directory listing shows them. Alternate data streams are not counted.

#### How it scans (`method`)

| `method` | What it does |
|----------|--------------|
| `auto` (default) | Reads the NTFS Master File Table when it can, and walks the folder tree otherwise. |
| `mft` | Reads the Master File Table, and fails (UNKNOWN, with the reason) when it cannot. |
| `walk` | Always walks the folder tree. |

Reading the **Master File Table** (Windows only) means one sequential read of
every file record on the volume, rather than a directory listing per folder. On
a large volume that is the difference between seconds and many minutes. It
needs a local NTFS volume and administrator rights to open the volume for raw
reads. The service account has those rights; an interactive `nscp test` that
is not elevated does not, and `auto` then walks instead. ReFS, FAT and exFAT
volumes and network shares are always walked. The legacy Windows XP build
leaves the Master File Table reader out: it always walks, and `method=mft`
returns UNKNOWN there.

Some things to know about the MFT scan:

* It reads the **whole volume's** table, however small the folder you asked
  about. That takes seconds, and memory while the scan runs: about 20 bytes
  per file on the volume (about 35 with `type=folders`), so a few hundred MB
  for a volume of ten million files. For a small folder, `method=walk` is
  faster.
* It sees the table **as written to disk**. NTFS writes changes to the table
  lazily, so a file created or grown in the last few seconds may not show up
  yet.
* A **hard-linked** file is counted once, under whichever of its links lies
  inside `path`. A walk counts it once per link. That is why a walk of
  `C:\Windows` puts `WinSxS` and `System32` well above their real disk usage:
  most of `System32` is hard links into `WinSxS`.

On **Linux** there is no filesystem-independent equivalent of the Master File
Table to read, so the check always walks, using `fts(3)`. `method=mft` returns
UNKNOWN there.

#### Links, junctions and mount points

Neither method follows a link. On Windows every reparse point that stands in
for another path is skipped: symbolic links, junctions, volume mount points and
WSL links. Deduplicated and cloud-backed files are reparse points too, but they
are real files and are counted. On Linux symbolic links are skipped, and so are
other filesystems mounted beneath `path`, unless you set
`same-filesystem=false`.

Folders the agent cannot open are skipped, and their contents are left out of
the totals.

#### Access control

`path` is held against `file access` in `[/settings/disk]`, the same gate as
`check_files` (see [Restricting what a check may read](../../concepts/check-access.md)).
Only the root is checked. Since neither scan follows a link, everything reported
lies beneath a root that passed.
