# Diagnostics

Diagnostics are small point-in-time tables of what is going on on the host
right now: the processes using the most CPU, the ones using the most memory,
the last failed checks. Modules keep them up to date and the core collects
them on an interval, the way it collects metrics. The latest diagnostics
snapshot is kept with every [failed check](failures.md) and travels with it to
an enrolled fleet server, so whoever reads a failure sees what was running
when it happened. These routes serve the same snapshot live.

* [List the sections](#list-the-sections) — `GET /api/v2/diagnostics`
* [Read one section](#read-one-section) — `GET /api/v2/diagnostics/{section}`

Facts say what the host *is*, metrics say *how much*, as numbers over time,
and diagnostics say *what*: rows and columns that name things - processes,
failed checks - which a metric key cannot carry without turning every process
name into a series of its own.

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

; The process tables (Windows).
[/settings/system/windows/diagnostics]
processes = true

; The process tables (Linux).
[/settings/system/unix/diagnostics]
processes = true
```

With `capture diagnostics` on and no table switched on in any module, the
snapshot holds only the core's own `failures.recent`. Every key is re-read on a
settings reload except `diagnostics interval`, which is read when the service
starts. A table switched off is absent from the next snapshot.

**Enabling a table enables what it needs.** `processes = true` turns on the
per-process sampling the tables are read off: on Windows the collector's
`process cpu` sample (one extra process-table query a second), on Linux a
read of `/proc/<pid>/stat` and `statm` for every process once a second. With
the table off none of it runs.

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
| `dropped` | Tables left out because the snapshot already held 16, or one of the same name. |
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

A column's `type` is `string`, `int`, `float`, `timestamp` or `bool`; `unit`
and `description` are only there when set. A section that could not be
collected carries an `error` and no rows: a failed table and an absent one
mean different things, the first that something is wrong, the second that it
is switched off.

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

* The `nscp test` prompt has `diagnostics` to list the tables and
  `diagnostics <section>` to print one.
* Modules read the snapshot through the `NSAPIDiagnosticsQuery` core API
  (`core_wrapper::get_diagnostics()`), which exchanges the `PB.Diagnostics`
  messages in `diagnostics.proto`. A module produces tables by declaring
  `"diagnostics": "produce"` in its `module.json` and implementing
  `fetchDiagnostics()` with the builder in `nscapi/nscapi_diagnostics_helper.hpp`.
* An enrolled agent [sends the diagnostics a failure was recorded with](../../setup/fleet.md#send-failure-context)
  to a fleet server that asks for failure context.
