# Failed checks

The core keeps a log of the last checks that did not come back OK — every
query it dispatched, from any transport (REST, NRPE, NSClient, the scheduler,
a check running other checks), whose result was WARNING, CRITICAL or UNKNOWN.
The log holds the result as the check produced it, with its performance data,
and the WEB server serves it so you can see *what has been failing on this
host* without going through the monitoring server.

* [List failed checks](#list-failed-checks) — `GET /api/v2/failures`
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
            ]
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
| `failures.list`   | `GET /api/v2/failures` |
| `failures.delete` | `DELETE /api/v2/failures` |

The log shows the output of every check the agent ran, for every caller, and
its arguments when `record arguments` is on. The built-in roles other than
`full` do not carry these privileges; give `failures.list` to a role of your
own for a monitoring console that should see them.

## Other ways in

Modules read the same log through the `NSAPIFailuresQuery` core API
(`core_wrapper::get_failures()` / `clear_failures()`), which exchanges the
`PB.Failures` protobuf messages in `failures.proto`.
