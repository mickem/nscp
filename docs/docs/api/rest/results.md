# Results

The WEB server can act as a *passive result cache*: it registers a submission
channel, keeps whatever is submitted to it in memory, and serves it back over
REST. This inverts the usual passive-monitoring flow — instead of the agent
pushing results out to a monitoring server it cannot always reach, the
monitoring server polls the results out of the agent whenever it likes.

Typical producers are the [Scheduler](../../reference/generic/Scheduler.md)
(scheduled checks submitted to a channel) and `check_and_forward` from
CheckHelpers, but anything that submits to a channel works.

* [List results](#list-results) — `GET /api/v2/results`
* [Fetch one result](#fetch-one-result) — `GET /api/v2/results/{key}`
* [Delete results](#delete-results) — `DELETE /api/v2/results`

**The cache is off by default.** Until `enabled` is set the web server
registers no channel, caches nothing, and answers every endpoint below with
`503 Service Unavailable`.

Only one result is kept per key. When a second result arrives for a key,
[`mode`](#which-result-is-kept) decides which of the two survives.

## Configuration

The feature is configured under `/settings/WEB/server/results`:

```ini
[/settings/WEB/server/results]
; Off by default: no channel is registered and the endpoints answer 503.
enabled = true
; The channel to listen on.
channel = WEB
; The key a result is cached under.
primary index = ${host}/${alias-or-command}
; Which of two results for the same key survives: last or worst.
mode = last
; Whether GET /api/v2/results removes the results it returns.
clear on poll = true
; How many distinct keys to keep.
max entries = 1000
; Drop results not updated for this many seconds (0 = keep them).
max age = 0
```

`primary index` may contain literal text and any of these variables:

| Variable              | Value                                              |
|-----------------------|----------------------------------------------------|
| `${host}`             | The submitting host, as named in the request header |
| `${source}`           | The raw sender id the host was resolved from        |
| `${channel}`          | The channel the result arrived on                   |
| `${command}`          | The command that produced the result                |
| `${alias}`            | The alias the result was submitted under            |
| `${alias-or-command}` | `${alias}` if set, otherwise `${command}`           |

An unknown variable is refused at load time (with an error in the log) and the
default expression is used instead, so a typo cannot silently collapse every
result into a single entry.

To submit results into the cache, point a producer at the channel:

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
channel = WEB
interval = 5m
```

`max entries` and `max age` only exist to bound memory when results arrive
under ever-changing keys. Results are never hidden merely for being stale —
every result carries an `age` — so `max age` is about memory, not about
deciding what counts as current.

### Which result is kept

| `mode`           | Behaviour                                                        |
|------------------|------------------------------------------------------------------|
| `last` (default) | The newest result wins, so a recovery replaces the problem before it. |
| `worst`          | The most severe result wins, so a problem is still reported even if it recovered before the next poll. An equally severe result still replaces, keeping the message current. |

Severity is ordered as everywhere else in NSClient++ — a plain numeric
comparison of the Nagios status, so `OK` < `WARNING` < `CRITICAL` < `UNKNOWN`.

Every entry carries both timestamps this distinction needs: `last_seen` is
when the key last submitted *anything* (so `age` still says whether the check
is reporting, even while a problem is held), and `result_seen` is when the
result actually being served arrived. In `last` mode the two are identical.

### Polling and draining

With `clear on poll = true` (the default) `GET /api/v2/results` **removes the
results it returns**. The next poll therefore reports what has happened since
the previous one rather than repeating it, which is what makes `mode = worst`
mean *worst since the last poll* — a check that went CRITICAL and recovered
between two polls is reported once, and then stops being reported.

Only what a poll actually returns is dropped, so a filtered poll
(`?status=critical`) cannot discard results its caller never saw. The response
carries `X-Result-Drained: true` when the poll consumed its results.

Set `clear on poll = false` when several consumers poll the same agent, or for
a dashboard that must not consume what it displays; results then stay until
they are replaced, expire, or are deleted. Fetching a single result by key is
a lookup rather than a poll and never drains, in either setting.

### Privileges

The endpoints are guarded by three privileges, none of which are part of any
bundled role except `full`. Grant them explicitly to whoever polls:

```
nscp web add-role --role poller --grant results.list,results.get,login.get
```

## List results

Returns every cached result, sorted by key.

| Key       | Value             |
|-----------|-------------------|
| Verb      | GET               |
| Address   | /api/v2/results   |
| Privilege | results.list      |

With `clear on poll = true` (the default) this **drains** what it returns —
see [Polling and draining](#polling-and-draining).

### Parameters

All filters are optional and combined with AND. `channel`, `host`, `command`
and `alias` match exactly (case-insensitively) — `?host=srv1` does not also
return `srv10`.

| Parameter | Description                                                            |
|-----------|------------------------------------------------------------------------|
| `channel` | Only results that arrived on this channel                              |
| `host`    | Only results from this host                                            |
| `command` | Only results for this command                                          |
| `alias`   | Only results submitted under this alias                                |
| `status`  | Comma-separated list of `ok`, `warning`, `critical`, `unknown` (or `0`–`3`) |

An unrecognised `status` value is answered with `400 Bad Request` rather than
being ignored.

### Response

```json
[
    {
        "key": "srv1/check_drivesize",
        "index": 42,
        "channel": "WEB",
        "host": "srv1",
        "source": "srv1",
        "command": "check_drivesize",
        "alias": "",
        "status": 0,
        "result": "OK",
        "message": "OK All 2 drive(s) are ok",
        "perf": "'/ used'=12.5GB;40;45;0;50",
        "count": 17,
        "first_seen": 1757145000,
        "last_seen": 1757145900,
        "result_seen": 1757145900,
        "first_seen_date": "2026-09-06 10:30:00",
        "last_seen_date": "2026-09-06 10:45:00",
        "result_seen_date": "2026-09-06 10:45:00",
        "age": 12,
        "result_url": "https://localhost:8443/api/v2/results/srv1/check_drivesize"
    }
]
```

| Field                | Description                                                      |
|----------------------|------------------------------------------------------------------|
| `key`                | The cache key, built from `primary index`                        |
| `index`              | Bumped on every update; the lowest index is the stalest entry    |
| `status` / `result`  | Nagios status as a number (0–3) and as a word                    |
| `perf`               | Performance data in Nagios plugin format, empty when there is none |
| `count`              | How many times this key has reported since it was first seen, including submissions `worst` mode suppressed |
| `first_seen`         | When the key was first cached, seconds since the unix epoch      |
| `last_seen`          | When the key last submitted anything                             |
| `result_seen`        | When the result being served arrived; equals `last_seen` in `last` mode |
| `*_date`             | The same instants as local time, for humans                      |
| `age`                | Seconds since the key last reported                              |

The number of results returned is sent as an `X-Result-Count` header, and
`X-Result-Drained` says whether the poll consumed them.

### Example

```
curl -s -k -u admin "https://localhost:8443/api/v2/results?status=warning,critical" | python -m json.tool
```

## Fetch one result

| Key       | Value                   |
|-----------|-------------------------|
| Verb      | GET                     |
| Address   | /api/v2/results/{key}   |
| Privilege | results.get             |

The key is used verbatim as the rest of the path, so a key containing `/`
(as the default `${host}/${alias-or-command}` produces) works unescaped. An
unknown or expired key is answered with `404 Not Found`. This is a lookup
rather than a poll: it never drains, whatever `clear on poll` says.

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/results/srv1/check_drivesize
```

## Delete results

Drops cached results. Use this to reset the cache after a maintenance window,
or to retire a key that will never report again.

| Key       | Value                                        |
|-----------|-----------------------------------------------|
| Verb      | DELETE                                        |
| Address   | /api/v2/results or /api/v2/results/{key}      |
| Privilege | results.delete                                |

Deleting the collection empties it; deleting a single key that is not cached
is answered with `404 Not Found`. Emptying the cache also resets what
`mode = worst` is holding, so the next result for a key starts a fresh
worst-of.

### Response

```json
{ "removed": 12 }
```

### Example

```
curl -s -k -u admin -X DELETE https://localhost:8443/api/v2/results
curl -s -k -u admin -X DELETE https://localhost:8443/api/v2/results/srv1/check_drivesize
```
