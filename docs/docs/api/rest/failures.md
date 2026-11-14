# Failed checks

The core keeps a log of the last checks that did not come back OK — every
query it dispatched, from any transport (REST, NRPE, NSClient, the scheduler,
a check running other checks), whose result was WARNING, CRITICAL or UNKNOWN.
The log holds the result as the check produced it, with its performance data,
and the WEB server serves it so you can see *what has been failing on this
host* without going through the monitoring server.

* [List failed checks](#list-failed-checks) — `GET /api/v2/failures`
* [Metrics at the time of the failure](#metrics-at-the-time-of-the-failure) — `GET /api/v2/failures/{index}/context`
* [Clear the log](#clear-the-log) — `DELETE /api/v2/failures`

The web UI shows the same log on its **Failed checks** page and, while
something has failed, as a widget on the dashboard.

**The log is on by default** and lives in memory: it starts empty when the
service starts and is never written to disk. It also records what the core
itself refuses — an unknown command, a check a caller was not allowed to run,
a check nested too deep — and a check whose module returned no answer at all,
so a check that "does nothing" on the monitoring side still shows up here.

## Configuration

The log is configured under `/settings/core/failed checks`. Every key takes
effect on a settings reload; turning the log off empties it.

```ini
[/settings/core/failed checks]
; Record checks that come back WARNING, CRITICAL or UNKNOWN.
enabled = true
; How many of the latest failures to keep (1-10000).
max entries = 50
; Keep the arguments the check was called with. Off by default: arguments
; can carry secrets, and the log is readable by anyone with failures.list.
record arguments = false
; Which results count as failed.
statuses = warning,critical,unknown
; Keep the agent's latest metrics snapshot with each failure.
capture metrics = false
; The most the snapshots may take, in bytes (65536 - 268435456).
metrics context budget = 4194304
; The largest single snapshot kept, in bytes (never more than the budget).
max context size = 1048576
; Trim each snapshot before it is kept: zeros, empty, duplicates (none by default).
prune metrics =
; Leave out every metric whose key contains one of these (comma-separated).
prune pattern =
```

`statuses` accepts the words (`warning`, `critical`, `unknown`) and the
Nagios numbers (`1`, `2`, `3`); OK results are never recorded. Lowering
`max entries` drops the oldest entries at once.

### What is stored

Each entry is the check's response: its command and alias, status, every
output line with its performance data, and who asked for it. Two things are
deliberately **not** stored as they were:

* **Arguments** are left out unless `record arguments` is on. When they are
  recorded, the value of any argument whose name contains `password` or
  `token` is replaced by `***`, in both the `password=secret` and the
  `--password secret` shape.
* **Secrets echoed in the message** are redacted the same way: a check that
  rejects its options repeats them ("unrecognised option 'password=secret'"),
  and that pair becomes `password=***` whether or not arguments are recorded.

Each entry is also capped (100 lines of 4096 characters, 100 performance
values per line, 100 arguments of 1024 characters) and the opaque `data` blob
some modules attach is dropped.

### Metrics context

With `capture metrics = true`, each failure also refers to the agent's latest
metrics snapshot: everything [`/api/v2/metrics`](metrics.md) was serving when
the check failed (CPU, memory, disks, network, whatever the loaded modules
publish). The agent collects one every `metrics interval` under
`[/settings/core]` (10 seconds by default), so the snapshot is at most that much
older than the failure.

A snapshot is shared, not copied. Every failure recorded within one interval
refers to the same one, and `metrics context budget` counts each distinct
snapshot once. When the budget is used up, the oldest snapshots are released
first. Their failures stay in the log, marked `evicted`. A snapshot larger than
`max context size` is not kept at all (`too_large`). On a Windows host running
CheckSystem, CheckDisk and CheckNet, a snapshot is about 70 KB (some 500
metrics), so the default budget holds about 50 snapshots and covers a
default-sized log in full. Turning capture off releases every snapshot held.

#### Pruning the snapshot

Most of a snapshot is usually noise for whoever reads it next to a failure: a
dozen idle WAN miniports with every rate at 0, adapters with no address, a
`_Total` disk that is the only disk. `prune metrics` trims the snapshot before it
is kept, so the log holds less, and so does the fleet report. It is a
comma-separated list of rules, all off by default:

| Rule         | Drops |
|--------------|-------|
| `zeros`      | Every numeric value (gauge, counter) that is exactly `0`. |
| `empty`      | Every string value that is empty. |
| `duplicates` | Every instance (a disk, an adapter, a core) whose values are all identical to an earlier instance in the same section. The first is kept. |

```ini
[/settings/core/failed checks]
capture metrics = true
prune metrics = zeros, empty, duplicates
```

Duplicates are compared on the values as collected, before `zeros` and `empty`
run, so two instances only collapse when they really were identical. An
instance is what a module publishes with a label (`disk`, `nic`, `core`, ...);
a metric without one is never collapsed. What was taken out is returned with
the snapshot as `pruned`, including which instance each collapsed one is
identical to, so `disk.io.C:` is not lost when it matches `disk.io._Total`.
To leave out what you already know is noise on your hosts, list it in
`prune pattern`. Every metric whose key (as `/api/v2/metrics` shows it)
contains one of the entries is dropped. The match ignores the case of A-Z, and
it is on the whole key, so an entry can name an instance, a metric, or a
whole section:

```ini
[/settings/core/failed checks]
prune pattern = HarddiskVolume8, Hyper-V Virtual, VirtualBox, .MACAddress
```

Patterns are applied before `prune metrics`, so an excluded instance is never
the one a duplicate is collapsed into.

Only the copy kept with a failure is pruned: [`/api/v2/metrics`](metrics.md)
and the metrics submitters always see the whole snapshot. A changed rule
applies from the next snapshot on.

The listing only says whether an entry has a snapshot (its `context` field).
The snapshot itself is served by
[its own endpoint](#metrics-at-the-time-of-the-failure), so a list of fifty
failures does not carry fifty snapshots.

## List failed checks

```
GET /api/v2/failures
```

Privilege: `failures.list`

Returns the log, newest first, inside an envelope that also says how the log
is configured — so a client can tell "nothing has failed" from "recording is
off" or "arguments are not recorded":

```
$ curl -k -s -u admin "https://localhost:8443/api/v2/failures?limit=1" | python -m json.tool
{
    "enabled": true,
    "max_entries": 50,
    "arguments_recorded": false,
    "statuses": "warning,critical,unknown",
    "last_index": 12,
    "dropped": 0,
    "capture_metrics": true,
    "context_budget": 4194304,
    "max_context_size": 1048576,
    "context_bytes_used": 72650,
    "count": 1,
    "entries": [
        {
            "index": 12,
            "timestamp": 1793426271,
            "date": "2026-Oct-31 06:57:51",
            "age": 42,
            "subject": "NRPEServer",
            "depth": 0,
            "command": "check_drivesize",
            "alias": "",
            "status": 2,
            "result": "CRITICAL",
            "message": "CRITICAL C: 92% used",
            "lines": [
                {
                    "message": "CRITICAL C: 92% used",
                    "perf": {
                        "C: used %": {
                            "value": 92,
                            "unit": "%",
                            "warning": 80,
                            "critical": 90,
                            "minimum": 0,
                            "maximum": 100
                        }
                    }
                }
            ],
            "context": {
                "state": "captured",
                "snapshot_id": 15,
                "captured_at": 1793426266,
                "age_at_failure": 5,
                "size": 72650
            }
        }
    ]
}
```

| Field                | Description |
|----------------------|-------------|
| `enabled`            | Whether the log is recording. `false` with an empty `entries` means switched off, not idle. |
| `max_entries`        | The cap, as configured. |
| `arguments_recorded` | Whether entries carry `arguments`. |
| `statuses`           | The statuses recorded, as configured. |
| `last_index`         | The highest `index` handed out so far; `0` before the first failure. Keeps counting across evictions and clears. |
| `dropped`            | Entries the cap has evicted since the service started. |
| `capture_metrics`    | Whether failures are recorded with a metrics snapshot. |
| `context_budget`, `max_context_size` | The snapshot budget and size cap in bytes, as configured (after clamping). |
| `context_bytes_used` | Bytes of distinct snapshots held right now. |
| `count`              | Number of `entries` in this answer (also in the `X-Result-Count` header). |

Each entry:

| Field       | Description |
|-------------|-------------|
| `index`     | Increasing, unique for the life of the process. Use it with `since`. |
| `timestamp` | When the check finished, as a Unix epoch; `date` is the same in the agent's local time and `age` the seconds since. |
| `subject`   | Who dispatched the check: the module and, when it knows, the principal (`WEBServer:admin`, `NRPEServer`, `Scheduler`). |
| `depth`     | `0` for a check a client asked for; `1` and up for a check another check ran on its behalf (`check_multi`, an alias with sub-checks). The parent is listed too, with the combined result. |
| `command`, `alias` | The command as the check reported it, and the alias it was called by, if any. |
| `status`, `result` | The Nagios number and word. |
| `message`   | The first output line, for a table that shows one line per row. |
| `lines`     | Every output line with its `perf` object, in the same shape as [`/api/v2/queries/{name}/commands/execute`](queries.md#command-execute). |
| `arguments` | The arguments the check was called with, redacted. **Only present when `arguments_recorded` is true**; an entry recorded while it was off has no key, so an empty list means "none were given". |
| `context`   | The metrics snapshot the entry refers to; see below. |

`context.state` is one of:

| State       | Meaning |
|-------------|---------|
| `captured`  | The snapshot is held; fetch it from [`/api/v2/failures/{index}/context`](#metrics-at-the-time-of-the-failure). |
| `disabled`  | `capture metrics` was off when the entry was recorded. |
| `evicted`   | The snapshot was released to stay within `metrics context budget`, or capture was turned off since. |
| `too_large` | The snapshot was larger than `max context size` and was not kept. |
| `none`      | Capture was on but the agent had not collected a snapshot yet (the first `metrics interval` after a start). |

Except for `disabled` and `none`, `context` also carries `snapshot_id` (two
entries with the same id saw the same snapshot), `captured_at` (epoch),
`age_at_failure` (seconds between the snapshot and the failure) and `size`
(the snapshot's serialised size, which is what the budget counts).

### Filtering

| Parameter  | Description |
|------------|-------------|
| `status`   | Comma-separated statuses to include, as words or numbers: `?status=critical`, `?status=1,2`. Anything else is a `400`. |
| `command`  | Only entries whose command or alias equals this, case-insensitively. |
| `since`    | Only entries with an `index` greater than this. A poller remembers the last `index` it saw and asks for what came after; if `last_index` moved further than the entries it got, the cap evicted some in between. |
| `limit`    | At most this many entries, after the other filters, newest first. |

```
curl -k -s -u admin "https://localhost:8443/api/v2/failures?status=critical&since=12&limit=20"
```

## Metrics at the time of the failure

```
GET /api/v2/failures/{index}/context
```

Privilege: `failures.list`

The metrics snapshot the entry was recorded with, in the same shape as
[`/api/v2/metrics?meta=1`](metrics.md): `metrics` is the flat key/value list and
`metadata` describes each key (type, help, unit, labels). The keys are the
ones `/api/v2/metrics` uses, so one parser reads both.

```
$ curl -k -s -u admin "https://localhost:8443/api/v2/failures/12/context" | python -m json.tool
{
    "state": "captured",
    "snapshot_id": 15,
    "captured_at": 1793426266,
    "age_at_failure": 5,
    "size": 72650,
    "index": 12,
    "timestamp": 1793426271,
    "metrics": {
        "system.cpu.total.idle": 3,
        "system.mem.physical.used": 15032385536,
        "workers.jobs": 214
    },
    "metadata": {
        "system.cpu.total.idle": { "type": "gauge", "help": "...", "unit": "percent" },
        "system.mem.physical.used": { "type": "gauge", "help": "...", "unit": "bytes" },
        "workers.jobs": { "type": "counter", "help": "..." }
    },
    "pruned": {
        "rules": "zeros,empty,duplicates",
        "zeros": 212,
        "empty": 96,
        "duplicates": 51,
        "patterns": ["harddiskvolume8", "hyper-v virtual", "virtualbox"],
        "excluded": 42,
        "collapsed": {
            "disk.io.C:": "disk.io._Total",
            "disk.io.HarddiskVolume1": "disk.io.D:",
            "system.network.WAN Miniport (IPv6)": "system.network.WAN Miniport (IP)"
        }
    }
}
```

`pruned` is only there when [`prune metrics` or `prune pattern`](#pruning-the-snapshot)
is set. Its counts are metrics dropped by each rule, `excluded` the ones
`patterns` matched, and `collapsed` maps each dropped instance to the instance
it was identical to.

| Parameter | Description |
|-----------|-------------|
| `meta`    | `?meta=0` leaves out `metadata`. |

An entry without a snapshot (`disabled`, `evicted`, `too_large`, `none`)
answers `200` with its `state` and no `metrics`: the entry exists, there is
just nothing to show. An index the log no longer holds (never recorded,
evicted by `max entries`, cleared) is a `404`, and one that is not a positive
number is a `400`.

## Clear the log

```
DELETE /api/v2/failures
```

Privilege: `failures.delete`

Empties the log and answers with the same envelope, plus how many entries went:

```
$ curl -k -s -u admin -X DELETE https://localhost:8443/api/v2/failures
{"enabled":true,"max_entries":50,"arguments_recorded":false,"statuses":"warning,critical,unknown","last_index":12,"dropped":0,"removed":12}
```

`last_index` is not reset, so a poller's `since` stays valid across a clear.

## Privileges

| Privilege         | Grants |
|-------------------|--------|
| `failures.list`   | `GET /api/v2/failures`, `GET /api/v2/failures/{index}/context` |
| `failures.delete` | `DELETE /api/v2/failures` |

The log shows the output of every check the agent ran, for every caller, and
its arguments when `record arguments` is on, and with `capture metrics` on
the metrics at the time, which `/api/v2/metrics` only serves to `metrics.list`.
The built-in roles other than
`full` do not carry these privileges; give `failures.list` to a role of your
own for a monitoring console that should see them.

## Other ways in

Modules read the same log through the `NSAPIFailuresQuery` core API
(`core_wrapper::get_failures()` / `get_failure_context()` /
`clear_failures()`), which exchanges the `PB.Failures` protobuf messages in
`failures.proto`.

An enrolled agent also [reports failed checks](../../setup/fleet.md#report-failed-checks)
to a fleet server that asks for them, at most once per
`[/settings/fleet] failure context interval`.
