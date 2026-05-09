# Metadata

The metadata API exposes information about what the running NSClient++
instance knows how to do — performance counters that can be checked,
submission channels that have a handler, and so on. Use it to discover
what the server can offer rather than to read live values (see
[Metrics](metrics.md) for that).

* [Index](#index)
* [Counters](#counters)
* [Channels](#channels)

The metadata controller is mounted on both `/api/v1/metadata` and
`/api/v2/metadata`. The examples below use `/api/v2`, but `/api/v1`
accepts the same payloads.

## Index

Returns the list of metadata resources exposed by this server, with a
`url` link to each one. New resources may be added in future releases —
clients should follow the URLs rather than hard-coding them.

| Key       | Value              |
|-----------|--------------------|
| Verb      | GET                |
| Address   | /api/v2/metadata   |
| Privilege | metadata.list      |

### Request

```
GET /api/v2/metadata
```

### Response

```json
[
    {
        "name": "counters",
        "title": "Performance counters",
        "url": "https://localhost:8443/api/v2/metadata/counters"
    },
    {
        "name": "channels",
        "title": "Registered submission channels",
        "url": "https://localhost:8443/api/v2/metadata/channels"
    }
]
```

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/metadata | python -m json.tool
```

## Counters

Returns the list of Windows performance counters that the `CheckSystem`
module has discovered on this host. The response is forwarded verbatim
from `CheckSystem pdh --list --json --all --no-instances`, so the shape
matches the CLI output of that command.

This endpoint requires the `CheckSystem` module to be loaded; if it is
not, the endpoint returns `500` with a body describing the missing module.

On non-Windows hosts the endpoint returns an empty list.

| Key       | Value                       |
|-----------|-----------------------------|
| Verb      | GET                         |
| Address   | /api/v2/metadata/counters   |
| Privilege | metadata.get.counters       |

### Request

```
GET /api/v2/metadata/counters
```

### Response

```json
[
    {
        "name": "\\Processor(_Total)\\% Processor Time",
        "type": "double"
    },
    {
        "name": "\\Memory\\Available Bytes",
        "type": "large"
    }
]
```

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/metadata/counters | python -m json.tool
```

## Channels

Returns the list of submission channels currently registered with the
core, together with the modules that handle each one. A channel is the
target name passed to `submit_message` (for example `"NSCA"` or
`"GraphiteOut"`); when a passive check, scheduled job, or external script
submits a result it names the channel and the core dispatches the payload
to the listening module(s).

| Key       | Value                       |
|-----------|-----------------------------|
| Verb      | GET                         |
| Address   | /api/v2/metadata/channels   |
| Privilege | metadata.get.channels       |

### Request

```
GET /api/v2/metadata/channels
```

### Response

```json
[
    { "name": "NSCA",       "plugins": ["NSCAClient"] },
    { "name": "GraphiteOut","plugins": ["GraphiteClient"] },
    { "name": "submit",     "plugins": ["Op5Client", "GraphiteClient"] }
]
```

`plugins` is an array because the core genuinely allows multiple modules
to register on the same channel — when that happens, every submission to
that channel is delivered to all listeners. In typical deployments each
channel has exactly one listener, but consumers must handle the
multi-listener case.

The two built-in pseudo-channels handled directly by the core
(`noop` discards the payload, `log` writes it to the NSClient++ log)
are not registered listeners and do **not** appear in this list. They
remain valid submission targets regardless.

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/metadata/channels | python -m json.tool
```
