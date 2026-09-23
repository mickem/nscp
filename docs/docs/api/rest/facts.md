# Facts

Host facts are the inventory the agent keeps about the machine it runs on —
what OS it is, what hardware it sits on, what it has attached. The core
collects them from the loaded modules on a schedule and stores one document
per host.

* [Read the inventory](#read-the-inventory) — `/api/v2/facts`, whole document or one subtree
* [Collect now](#collect-now) — `/api/v2/facts/commands/refresh`

## Facts or tags?

Both describe the host, and they answer different questions — pick by what you
are going to do with the answer.

| | Tags (`/api/v2/tags`) | Facts |
|---|---|---|
| Shape | flat `key=value` strings | a document: objects, lists, numbers, strings |
| Question | *which group is this host in* | *what is this host* |
| Collected | always | only once a set is enabled |
| Uploaded to a fleet server | yes, on every state report | no |

A fleet selector matches a tag whole, which is why `os_family` and `arch` are
tags. A list of volumes, or a vendor and model, is not something a selector
matches on — that is what a fact set is for, and a comma-joined tag value is
where that shape goes to die.

## Enabling a fact set

Nothing is collected until you turn a set on, because an inventory is data an
operator did not necessarily agree to ship. A set is enabled in **the module
that produces it**, beside that module's other settings — so a set can never
be enabled with nothing to answer for it:

```ini
[/settings/system/windows/facts]
os = true
hardware = true
```

`[/settings/system/unix/facts]` on Linux, with the same keys. Which sets exist
is part of each producing module's reference documentation. The collection
interval is `[/settings/facts] interval` (an hour by default).

Turning a set off stops the module returning it, and the core then drops it
from the document — there is nothing to clean up.

## Read the inventory

| Key       | Value            |
|-----------|------------------|
| Verb      | GET              |
| Address   | /api/v2/facts    |
| Privilege | facts.get        |

### Request

```
GET /api/v2/facts
```

Optionally `?path=<dotted path>` to fetch one subtree: `?path=os`,
`?path=os.family`, `?path=storage.volumes`.

### Response

```json
{
  "revision": 7,
  "collected": "2026-09-23T10:00:00Z",
  "path": "",
  "found": true,
  "enabled": ["hardware", "os"],
  "errors": {},
  "facts": {
    "hardware": {
      "cpu_cores": 20,
      "manufacturer": "Dell Inc.",
      "memory_gb": 32,
      "model": "PowerEdge R650"
    },
    "os": {
      "arch": "x86_64",
      "family": "windows",
      "name": "Windows Server 2022",
      "version": "10.0.20348",
      "virtualization": "none"
    }
  }
}
```

| Field       | Meaning |
|-------------|---------|
| `revision`  | Monotonic change counter. `0` is the empty document a fresh install reports, and it only moves when the stored facts actually change — so a poller can skip a body it has already seen. |
| `collected` | When the last round *completed*, ISO 8601 UTC. A round with nothing enabled completes like any other, so this carries a timestamp even when `facts` is empty. |
| `path`      | Echoed back from the request; `""` for the whole document. |
| `found`     | Whether anything produced what was asked for. |
| `enabled`   | The sets the loaded producers are configured to collect, including one that is enabled and currently failing. |
| `errors`    | Per-set collection errors from the last round, keyed by set id. |
| `facts`     | The document, or the subtree at `path`. |

A field a producer could not determine is **omitted**, never written empty: an
absent key means unknown, where an empty string would read as an answer.

### A path nothing produced

Not an error — a client asking for a set the operator has not enabled should
render "not collected", not a failure:

```json
{ "revision": 7, "path": "storage", "found": false, "facts": {}, "…": "…" }
```

### A set that failed to collect

A set that is enabled but could not be collected keeps the value it last had
and is named under `errors`, so a transient failure never blanks the
inventory:

```json
{
  "enabled": ["hardware", "os"],
  "errors": { "hardware": "WMI query timed out" },
  "facts": { "hardware": { "…": "…" } }
}
```

The bundled web UI marks such a set **stale** and keeps showing its values.

## Collect now

Asks every loaded producer to collect immediately instead of waiting for the
next scheduled round, and answers with the document that round produced.

| Key       | Value                             |
|-----------|-----------------------------------|
| Verb      | POST                              |
| Address   | /api/v2/facts/commands/refresh    |
| Privilege | facts.refresh                     |

### Request

```
POST /api/v2/facts/commands/refresh
```

### Response

The same body as [Read the inventory](#read-the-inventory).

Collecting is real work on the host, so this has its own grant rather than
riding on `facts.get` — a read-only caller can watch the inventory without
being able to drive collection in a loop.

## In the agent and the UI

The same document is available without HTTP:

* `nscp test` has a `facts` command (`facts`, `facts os.family`,
  `facts refresh`) — see [Test mode](../../concepts/test-mode.md).
* The bundled web UI has a **Facts** page, one card per set, with a refresh
  button that drives the endpoint above.
