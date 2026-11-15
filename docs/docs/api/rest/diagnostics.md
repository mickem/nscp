# Diagnostics

Diagnostics are small point-in-time tables of what is going on on the host
right now: the processes using the most CPU, the ones using the most memory,
the services that are not running as they should, the last errors written to
the system logs, the size of the places that usually fill a disk, the last
failed checks. Modules keep them up to date and the core collects
them on an interval, the way it collects metrics. The latest diagnostics
snapshot is kept with every [failed check](failures.md) and travels with it to
an enrolled fleet server, so whoever reads a failure sees what was running
when it happened. These routes serve the same snapshot live.

* [List the sections](#list-the-sections) — `GET /api/v2/diagnostics`
* [Read one section](#read-one-section) — `GET /api/v2/diagnostics/{section}`

Facts say what the host *is*, metrics say *how much*, as numbers over time,
and diagnostics say *what*: rows and columns that name things - processes,
failed checks, services, log entries - which a metric key cannot carry without
turning every process name into a series of its own.

## Turning it on

Nothing is collected until two things are switched on: the core's capture,
and each table in the module that produces it.

```ini
[/settings/core/failed checks]
; Collect diagnostics every interval, keep them with each failed check and
; serve them on /api/v2/diagnostics.
capture diagnostics = true
; The most rows kept per table (1-50).
diagnostics rows = 10

[/settings/core]
; How often to collect them. Empty (the default) collects them at the end of
; every metrics round, on the same thread.
diagnostics interval =

; Windows: the process tables and the services in trouble (CheckSystem)...
[/settings/system/windows/diagnostics]
processes = true
services = true

; ...the service crash events and the event log errors (CheckEventLog).
[/settings/eventlog/diagnostics]
services = true
errors = true

; Linux: all of them from CheckSystemUnix - services covers both service
; tables, errors the journal's errors.
[/settings/system/unix/diagnostics]
processes = true
services = true
errors = true

; Both: the temp and log directories, page and swap files and (Windows)
; shadow-copy storage, measured every growth spots interval (CheckDisk).
[/settings/disk/diagnostics]
growth spots = true
growth spots interval = 10m
```

With `capture diagnostics` on and no table switched on in any module, the
snapshot holds only the core's own `failures.recent`. Every key is re-read on a
settings reload except `diagnostics interval`, which is read when the service
starts. A table switched off is absent from the next snapshot.

**Enabling a table enables what it needs.** `processes = true` turns on the
per-process sampling the tables are read off: on Windows the collector's
`process cpu` sample (one extra process-table query a second), on Linux a
read of `/proc/<pid>/stat` and `statm` for every process once a second. With
the table off none of it runs. The service and log tables start a reader of
their own:

| Key | Starts |
|---|---|
| `services` (CheckSystem) | A refresh every 30 seconds on the collector's auxiliary thread: one walk of the service control manager, opening only the services whose status already looks wrong. |
| `services`, `errors` (CheckEventLog) | One thread with its own event log subscriptions (System for the service events; Application and System for the errors). They are separate from the [real-time filters](../../reference/windows/CheckEventLog.md), so they work whether or not `real-time` is enabled. |
| `services`, `errors` (CheckSystemUnix) | One thread that runs `systemctl` every 30 seconds - reading the services that look troubled each time, and every running service (for its restart count) once in five minutes, so a restart of a service that is running again can take that long to appear in `services.problems` - and reads the journal with `journalctl` every 10 seconds, from where the last read stopped. Each run is bounded to 10 seconds. The build does not link libsystemd. |
| `growth spots` (CheckDisk) | One thread that measures the growth spots 15 seconds after it starts and then every `growth spots interval` (default `10m`, at least `10s`; a settings reload keeps the last round and its schedule rather than measuring again): one directory at a time with a half-second pause between them, at background priority (thread background mode on Windows; the idle I/O class and nice 19 on Linux), at most 30 seconds per directory and 2 minutes of walking per round. |

An event table starts from one backward read of the last ten minutes of its
logs, so a check that fails soon after the agent started - or after a
settings reload, which restarts the readers - still has the events that led
up to it. After that each event is rendered once, as it is written, and the
last 50 are kept.

**Nothing is collected because a check failed or a request arrived.** A round
asks each module for what it already holds - the process table its collector
sampled a moment ago - and never queries, walks or waits. A failure attaches
the snapshot the last round left, at most one interval old, and these routes
serve it. A host that is failing checks is often bogged down; collecting on
failure would add load at the worst moment.

## The tables

| Section | Produced by | Enabled by | Columns |
|---|---|---|---|
| `failures.recent` | the core | `capture diagnostics` | `time`, `command`, `status`, `subject`, `message` |
| `processes.top_cpu` | CheckSystem (Windows), CheckSystemUnix (Linux) | `processes` | `name`, `pid`, `cpu` (percent), `working_set` (bytes), `private_bytes` (bytes) |
| `processes.top_memory` | CheckSystem (Windows), CheckSystemUnix (Linux) | `processes` | the same, sorted by working set |
| `services.problems` | CheckSystem (Windows), CheckSystemUnix (Linux) | `services` | `name`, `state`, `exit_code`, `binary_path`, `account`, `started` (timestamp), `restart_count` |
| `services.events` | CheckEventLog (Windows), CheckSystemUnix (Linux) | `services` | `time`, `service`, `id`, `message` |
| `eventlog.errors` | CheckEventLog (Windows), CheckSystemUnix (Linux) | `errors` | `time`, `log`, `source`, `id`, `level`, `message` |
| `disk.growth_spots` | CheckDisk | `growth spots` | `path`, `kind`, `size` (bytes), `note` |

* **`failures.recent`** is the last failed checks from the
  [failed-check log](failures.md), newest first: when, which command, its
  status, who asked for it and the first line of its message. In a failure's
  own context it holds the failures before that snapshot was taken - the
  run-up to it.
* **`processes.top_cpu`** ranks the processes by their CPU share over the last
  second, as a share of all cores together: 100 is every core busy, so a
  process pegging one core of four shows as 25. **`processes.top_memory`** ranks
  them by working set (resident memory). `private_bytes` is memory the process
  alone owns: private bytes on Windows, resident pages not shared with another
  process on Linux. A process is its name and pid: **never its command line**,
  which carries credentials far more often than check arguments do. On macOS
  the tables report that they are not supported yet.
* **`services.problems`** lists the services that are not where they should
  be. Modules are not told which service a failed check was about, so the
  table is every service in trouble, the ones restarted most often first,
  then those that exited with an error:
    * **Windows:** what `check_service` flags by default - an automatic
      service that is not running, unless it is delayed, trigger-started,
      still starting or stopped cleanly (exit code 0) - plus any service that
      stopped with an error other than "never started". An automatic service
      that was stopped by hand and exited with 0 is therefore not listed. `exit_code` is the Win32 exit code, or the service's own
      code when it reported one; `restart_count` is always `null`, because the
      service control manager keeps no such count.
    * **Linux:** the systemd units that failed, are waiting to be restarted,
      were restarted automatically (`NRestarts`, which systemd only resets
      when the unit is started or stopped by hand) or whose last run did not
      succeed. `state` is `ActiveState (SubState)`, `exit_code` the main
      process's exit status, `account` the unit's `User`.

    `binary_path` is the executable alone: the image path's or `ExecStart`'s
    arguments are cut off, because they can carry credentials. `started` is
    when the service's current process started, `null` when it has none.
* **`services.events`** holds the service manager's own account of services
  that crashed: on Windows the System log's Service Control Manager events
  7031 and 7034 (terminated unexpectedly) and 7024 (terminated with an
  error), with `service` translated from the display name in the event to the
  service's name; on Linux the systemd manager's journal messages for a unit
  whose start job failed, that failed, whose main process exited with an
  error, or whose restart was scheduled (`id` is `null`: a journal entry has
  none).
* **`eventlog.errors`** holds the last errors written to the system logs: on
  Windows every critical or error event in the Application and System logs;
  on Linux every journal entry at priority `err` or worse, with `log` the
  unit that wrote it (`kernel` for a kernel message, `journal` for an entry
  that belongs to no unit), `source` its syslog identifier and
  `level` one of `emerg`, `alert`, `crit` or `err`. The agent must be able to
  read the system journal: root, or a member of `adm` or `systemd-journal`.
  A message is event text, which applications fill with whatever they like;
  on Windows it is rendered in the host's language, and an event whose
  source has no message for its id shows the event's own strings.

* **`disk.growth_spots`** is the size of the places that usually fill a disk,
  for whoever reads a failed `check_drivesize` - the
  `disk.free.<drive>.used_change_1h` and `used_change_24h` metrics say which
  volume grew, this table where to look. `kind` is one of:

    | `kind` | Windows | Linux |
    |---|---|---|
    | `temp` | the agent's `TEMP` and the Windows `Temp` directory | `TMPDIR` (when set), `/tmp`, `/var/tmp` |
    | `logs` | the agent's log directory (`${log-path}`), the Windows `Logs` directory and the event logs (`System32\winevt\Logs`) | the agent's log directory, `/var/log` |
    | `page_file` | each page file the memory manager has open | - |
    | `swap_file` | `swapfile.sys` on the system drive | each swap file in `/proc/swaps` (a swap partition takes no filesystem space) |
    | `shadow_storage` | the shadow-copy storage in use, one row per volume (`Win32_ShadowStorage`) | - |

    Every path comes from the system (environment, registry, WMI), never a
    hard-coded drive letter, and a spot that does not exist has no row. A
    directory's `size` is the total of the files beneath it; links, junctions
    and mount points are not followed and the walk stays on the directory's
    filesystem. A directory that hits its time limit reports the size counted
    so far, says so in `note` and marks the table `truncated`; entries that
    cannot be read are skipped and counted in `note`, so an unelevated agent
    still reports what it can see. A row with no `size` says why in `note`:
    the folder itself could not be read, or - for the shadow storage, which
    needs an elevated agent - the query failed. `captured_at` is when the last
    round finished, so this table can be up to an interval older than the
    rest of the snapshot; a settings reload keeps it until the next round.

On Linux the three service and log tables need systemd: a host without it
(`/run/systemd/system`) produces none of them, macOS included.

## Caps

| Cap | Default |
|---|---|
| Rows per table | `diagnostics rows`, 10 (at most 50) |
| Cell length | 1024 bytes |
| Tables per snapshot | 16 |
| One snapshot | `max context size`, shared with metrics snapshots |

A table cut by a cap is marked `truncated`, so a reader can tell "10 rows" from
"at least 10". A top list is always truncated when there were more processes
than rows. Every text cell passes through the same redaction as the
failed-check log: a `password=` or `token=` pair loses its value. `prune
pattern` under `[/settings/core/failed checks]` applies to table names too:
`prune pattern = processes.top_memory` leaves that table out. The metrics
pruning rules (`zeros`, `empty`, `duplicates`) do not apply to tables.

## List the sections

```
GET /api/v2/diagnostics
```

Privilege: `diagnostics.list`

The tables in the latest snapshot, without their rows:

```
$ curl -k -s -u admin https://localhost:8443/api/v2/diagnostics | python -m json.tool
{
    "enabled": true,
    "snapshot_id": 812,
    "captured_at": "2026-11-15T10:42:07Z",
    "max_rows": 10,
    "dropped": 0,
    "excluded": [],
    "count": 3,
    "sections": [
        {
            "source": "core",
            "description": "The last failed checks, newest first",
            "captured_at": "2026-11-15T10:42:07Z",
            "truncated": false,
            "name": "failures.recent",
            "rows": 2
        },
        {
            "source": "CheckSystem",
            "description": "The processes using the most CPU over the last second",
            "captured_at": "2026-11-15T10:42:06Z",
            "truncated": true,
            "name": "processes.top_cpu",
            "rows": 10
        },
        {
            "source": "CheckSystem",
            "description": "The processes with the largest working set",
            "captured_at": "2026-11-15T10:42:06Z",
            "truncated": true,
            "name": "processes.top_memory",
            "rows": 10
        }
    ]
}
```

| Field | Description |
|---|---|
| `enabled` | Whether diagnostics are collected (`capture diagnostics`). `false` with no sections means switched off, not idle. |
| `snapshot_id` | Increasing for the life of the process; `0` before the first round. |
| `captured_at` | When the core finished the round. Each section has its own `captured_at`, which can be older: a table refreshed on a slow cadence is handed over as it was last refreshed. |
| `max_rows` | `diagnostics rows`, as applied. |
| `dropped` | Tables left out because the snapshot already held 16, or one of the same name, or because it did not fit `max context size` even with every row given up (the agent then also logs a warning). |
| `excluded` | Tables `prune pattern` left out. |
| `sections` | Each table's `name`, `source` (the module that produced it, or `core`), `description`, `captured_at`, `truncated`, `rows` (the row count) and, when it could not be collected, `error`. |

## Read one section

```
GET /api/v2/diagnostics/{section}
```

Privilege: `diagnostics.list`

One table, with its columns and its rows. Each row is an object keyed by column
name, so a client never counts positions. Timestamps are ISO 8601 UTC, and a
missing value is `null`.

```
$ curl -k -s -u admin https://localhost:8443/api/v2/diagnostics/processes.top_cpu | python -m json.tool
{
    "source": "CheckSystem",
    "description": "The processes using the most CPU over the last second",
    "captured_at": "2026-11-15T10:42:06Z",
    "truncated": true,
    "columns": [
        { "name": "name", "type": "string" },
        { "name": "pid", "type": "int" },
        { "name": "cpu", "type": "float", "unit": "percent", "description": "Share of all cores over the last second" },
        { "name": "working_set", "type": "int", "unit": "bytes", "description": "Resident memory" },
        { "name": "private_bytes", "type": "int", "unit": "bytes", "description": "Memory not shared with other processes" }
    ],
    "rows": [
        { "name": "acme-indexer.exe", "pid": 4242, "cpu": 87.5, "working_set": 412000256, "private_bytes": 398458880 },
        { "name": "svchost.exe", "pid": 1288, "cpu": 2.1, "working_set": 85983232, "private_bytes": 48349184 }
    ],
    "name": "processes.top_cpu",
    "snapshot_id": 812
}
```

The growth spots of an unelevated agent: the folder and the WMI class it may
not read say so in `note` rather than going missing.

```
$ curl -k -s -u admin https://localhost:8443/api/v2/diagnostics/disk.growth_spots | python -m json.tool
{
    "source": "CheckDisk",
    "description": "Size of the places that usually fill a disk: temp and log directories, page and swap files, shadow-copy storage",
    "captured_at": "2026-11-15T09:33:05Z",
    "truncated": false,
    "columns": [
        { "name": "path", "type": "string" },
        { "name": "kind", "type": "string", "description": "temp, logs, page_file, swap_file or shadow_storage" },
        { "name": "size", "type": "int", "unit": "bytes", "description": "The directory tree's files, the file, or the shadow storage in use" },
        { "name": "note", "type": "string", "description": "What limited the size (a time limit, unreadable entries), or why there is none; empty when complete" }
    ],
    "rows": [
        { "path": "C:\\Windows\\Temp", "kind": "temp", "size": null, "note": "Unable to read folder C:\\Windows\\Temp: 5: Access is denied." },
        { "path": "C:\\Windows\\Logs", "kind": "logs", "size": 600678673, "note": "1 folder or entry could not be read" },
        { "path": "C:\\Windows\\system32\\winevt\\Logs", "kind": "logs", "size": 435499008, "note": null },
        { "path": "C:\\pagefile.sys", "kind": "page_file", "size": 36507222016, "note": null },
        { "path": "C:\\swapfile.sys", "kind": "swap_file", "size": 16777216, "note": null },
        { "path": "all volumes", "kind": "shadow_storage", "size": null, "note": "could not be read (needs an elevated agent): ..." }
    ],
    "name": "disk.growth_spots",
    "snapshot_id": 9
}
```

A column's `type` is `string`, `int`, `float`, `timestamp` or `bool`; `unit`
and `description` are only there when set. A section that could not be
collected carries an `error`: a failed table and an absent one mean different
things, the first that something is wrong, the second that it is switched
off. An `error` does not rule out rows - `eventlog.errors` reads two logs, and
when one of them cannot be read the section names it in `error` and still
carries the rows from the other.

A section the snapshot does not hold is a `404`, as is any section while
`capture diagnostics` is off - the message says which.

## Privileges

| Privilege | Grants |
|---|---|
| `diagnostics.list` | `GET /api/v2/diagnostics`, `GET /api/v2/diagnostics/{section}` |

**No built-in role holds `diagnostics.list`** other than `full`, which holds
everything: the tables name what runs on the host, which is more than a
monitoring or metrics scraper needs. Give it to a role of your own:

```ini
[/settings/WEB/server/roles]
diagnostics = diagnostics.list,login.get
```

A caller with `failures.list` reads the diagnostics a failure was recorded
with through [`/api/v2/failures/{index}/context`](failures.md#metrics-at-the-time-of-the-failure),
as it already reads the metrics snapshot: grant `failures.list` with that in
mind once `capture diagnostics` is on.

## Other ways in

* The web UI's **Diagnostics** page shows every table of the latest snapshot,
  `failures.recent` first, with the refresh selector the Metrics page has. It
  reads these routes, so it needs `diagnostics.list`: a role without it is
  told so on the page. Above the tables, a *Diagnostics tables* card holds the
  switches: `capture diagnostics` and every table a loaded module can
  produce, each written to the settings of the module that owns it (so it
  needs the settings grants, and a change takes effect when the configuration
  is saved and reloaded). With capture off the page says how to turn it on. The
  **Failed checks** page shows the tables a failure was recorded with under
  `failures.list`.
* The `nscp test` prompt has `diagnostics` to list the tables and
  `diagnostics <section>` to print one.
* Modules read the snapshot through the `NSAPIDiagnosticsQuery` core API
  (`core_wrapper::get_diagnostics()`), which exchanges the `PB.Diagnostics`
  messages in `diagnostics.proto`. A module produces tables by declaring
  `"diagnostics": "produce"` in its `module.json` and implementing
  `fetchDiagnostics()` with the builder in `nscapi/nscapi_diagnostics_helper.hpp`.
* An enrolled agent [sends the diagnostics a failure was recorded with](../../setup/fleet.md#send-failure-context)
  to a fleet server that asks for failure context.
