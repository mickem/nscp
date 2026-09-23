# Facts

The facts API serves this host's **inventory**: the structured, hierarchical
description of the machine that modules contribute — which volumes exist, what
the hardware is, which software is installed.

* [What facts are](#what-facts-are)
* [Get facts](#get-facts)
* [Get one fact path](#get-one-fact-path)
* [Refresh](#refresh)

## What facts are

Facts are not [tags](../../reference/generic/WEBServer.md). A tag is one flat
`key=value` string that a fleet group selector matches on (`sqlserver =
"detected"`); a fact set is a subtree with lists of records in it. The two
exist side by side and neither replaces the other.

**Nothing is collected until a fact set is enabled.** A fresh install serves
an empty document from this endpoint, with an empty `enabled` list. Sets are
turned on per id in the configuration:

```ini
[/settings/facts]
os = true
storage.volumes = true
```

`nscp test` → `facts list` shows the set ids this agent can produce, what each
one contains and what it costs to collect.

### The envelope

Every response on this endpoint has the same shape:

| Field      | Meaning                                                                                         |
|------------|--------------------------------------------------------------------------------------------------|
| `revision` | Monotonic counter, bumped on every effective change. `0` on a host with nothing enabled.        |
| `hash`     | SHA-256 of the canonical document (keys sorted, no whitespace). Also the response's `ETag`.      |
| `enabled`  | The fact set ids currently enabled in `[/settings/facts]`.                                       |
| `errors`   | Per-set collection failures, keyed by fact set id. A set that could not be collected says so here rather than being silently absent. |
| `facts`    | The document itself (or, on a path request, the subtree at that path).                          |

The `hash` is what the fleet server compares to decide whether it needs the
document at all, so it is stable: two agents that collected the same inventory
produce the same hash whatever order their producers wrote it in.

## Get facts

Returns the whole facts document.

| Key       | Value           |
|-----------|-----------------|
| Verb      | GET             |
| Address   | /api/v2/facts   |
| Privilege | facts.get       |

### Example

```
GET /api/v2/facts
```

```json
{
  "revision": 12,
  "hash": "9f2c1a…",
  "enabled": ["os", "storage.volumes"],
  "errors": {},
  "facts": {
    "os": {
      "family": "linux",
      "name": "Ubuntu 24.04",
      "version": "6.8.0",
      "boot_time": "2026-09-01T04:12:09Z"
    },
    "storage": {
      "volumes": [
        { "id": "/", "fs": "ext4", "size_bytes": 255000000000, "type": "fixed" },
        { "id": "/boot", "fs": "vfat", "size_bytes": 1000000000, "type": "fixed" }
      ]
    }
  }
}
```

With nothing enabled the same request answers `200` with `"facts": {}`,
`"enabled": []` and the hash of the empty document — it does not 404. "No
inventory" and "no such endpoint" are different answers and a caller needs to
be able to tell them apart.

The response carries the document hash as its `ETag`, so a UI or a poller can
revalidate cheaply.

## Get one fact path

Returns the subtree at a dotted path — a fact set id (`os`,
`software.installed`) or anything inside one (`os.family`).

| Key       | Value                  |
|-----------|------------------------|
| Verb      | GET                    |
| Address   | /api/v2/facts/*path*   |
| Privilege | facts.get              |

A path is one or more `snake_case` components separated by single dots;
anything else is rejected with `400`. A well-formed path that nothing produced
answers `404`, which is how a caller tells "this host has no docker facts"
from "docker facts are empty".

### Example

```
GET /api/v2/facts/os.family
```

```json
{
  "revision": 12,
  "hash": "9f2c1a…",
  "enabled": ["os", "storage.volumes"],
  "errors": {},
  "path": "os.family",
  "found": true,
  "facts": "linux"
}
```

## Refresh

Runs a collection round now and returns the resulting document.

| Key       | Value                    |
|-----------|--------------------------|
| Verb      | POST                     |
| Address   | /api/v2/facts/refresh    |
| Privilege | facts.refresh            |

This is the one route on the endpoint that costs something: it asks every
producer to collect immediately, which for the expensive sets means re-reading
the installed-software hives or querying Windows Update. That is why it has a
grant of its own — the built-in `monitoring` role carries `facts.get` but not
`facts.refresh`, which only `full` has.

Facts are otherwise refreshed on a schedule (`[/settings/facts] interval`,
default `1h`), once at startup, and after a settings reload.
