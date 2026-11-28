# Facts

Host facts are the inventory the agent keeps about the machine it runs on —
what OS it is, what hardware it sits on, what it has attached. The core
collects them from the loaded modules on a schedule and stores one document
per host.

* [Read the inventory](#read-the-inventory) — `/api/v2/facts`, whole document or one subtree
* [Read an on-demand set](#read-an-on-demand-set) — `/api/v2/facts?path=scripts.python`, collected for the request
* [List the sets](#list-the-sets) — `/api/v2/facts/sets`
* [Collect now](#collect-now) — `/api/v2/facts/commands/refresh`

## Facts or tags?

Both describe the host, and they answer different questions — pick by what you
are going to do with the answer.

| | Tags (`/api/v2/tags`) | Facts |
|---|---|---|
| Shape | flat `key=value` strings | a document: objects, lists, numbers, strings |
| Question | *which group is this host in* | *what is this host* |
| Collected | always | only once a set is enabled |
| Uploaded to a fleet server | yes, on every state report | when enrolled: the hash on every report, the document when it changes |

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

`[/settings/system/unix/facts]` on Linux, with the same keys. Which sets exist,
which module produces each and what they contain is on the
[Host Facts](../../concepts/facts.md) page. The collection interval is
`[/settings/facts] interval` (an hour by default).

Turning a set off stops the module returning it, and the core then drops it
from the document — there is nothing to clean up.

## Read the inventory

| Key       | Value            |
|-----------|------------------|
| Verb      | GET              |
| Address   | /api/v2/facts    |
| Privilege | facts.get, or facts.get.*set* for one set |

### Request

```
GET /api/v2/facts
```

Optionally `?path=<dotted path>` to fetch one subtree: `?path=os`,
`?path=os.family`, `?path=storage.volumes`.

### Privileges

| Grant              | Reads |
|--------------------|-------|
| `facts.get`        | the stored document, whole or by path. What it has always read, and nothing more: it does **not** read an [on-demand set](#read-an-on-demand-set). |
| `facts.get.<set>`  | one set — the first segment of the path — and nothing else: `facts.get.os` reads `?path=os` and `?path=os.family`, and is refused the whole document and every other set. The only grant that reads an on-demand set. |

Grants match segment by segment, so `facts.get` does not imply
`facts.get.os`, and a role holding only per-set grants cannot read the
document without a path. A wildcard does cover them: `facts.*` (and `*`, the
`full` role) reads every set, on-demand ones included.

A per-set grant is how a role is given part of the inventory — the OS and the
hardware, say, without the list of installed software:

```ini
[/settings/WEB/server/roles]
helpdesk = login.get,facts.get.os,facts.get.hardware
```

### Response

```json
{
  "revision": 7,
  "collected": "2026-09-23T10:00:00Z",
  "path": "",
  "found": true,
  "enabled": ["hardware", "os"],
  "errors": {},
  "gathered": {
    "hardware": "2026-09-23T06:12:41Z",
    "os": "2026-09-23T06:12:41Z"
  },
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
| `collected` | When the last round *completed*, ISO 8601 UTC — when the core last **asked**. A round with nothing enabled completes like any other, so this carries a timestamp even when `facts` is empty. |
| `gathered`  | When each set's values were **read off the machine**, keyed by set id. This is the one to show a reader; see below. A set whose producer did not say is absent, and the round time is then the right answer for it. |
| `path`      | Echoed back from the request; `""` for the whole document. |
| `found`     | Whether anything produced what was asked for. |
| `enabled`   | The sets the loaded producers are configured to collect, including one that is enabled and currently failing. |
| `errors`    | Per-set collection errors from the last round, keyed by set id. |
| `facts`     | The document, or the subtree at `path`. |

A field a producer could not determine is **omitted**, never written empty: an
absent key means unknown, where an empty string would read as an answer.

### `collected` is not `gathered`

A producer is free to hand back a snapshot it took earlier rather than collect
again — that is what `reason` in the request is for, and what a producer of an
expensive set should do. When it does, the round that delivered the values has
nothing to do with how old they are.

`CheckSystem` works exactly this way: what OS a host runs and what hardware it
sits on cannot change while the process runs, so it reads once at startup and
reports that reading thereafter. An hourly round then moves `collected` while
`gathered` stays where it was — and `revision` does not move at all, because
the document did not change.

**Show `gathered` against the values.** Showing `collected` claims an
inventory is as fresh as the last time anyone asked, which for a cached
producer is not true. `POST /api/v2/facts/commands/refresh` is what makes a
producer read for real.

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

## Read an on-demand set

Some sets are not in the stored document at all. A module collects them when
their path is read, the core keeps the answer for a few seconds, and nothing
is stored, counted against `max size`, hashed into `revision` or sent to a
fleet server. They exist for listings that are too large or too volatile to
keep: what is in each script runtime's folder, and the Windows performance
counter catalog.

| Key       | Value                          |
|-----------|--------------------------------|
| Verb      | GET                            |
| Address   | /api/v2/facts?path=*set*       |
| Privilege | facts.get.*set* (`facts.get` alone is refused) |

| Path                         | Produced by           | Contains |
|------------------------------|-----------------------|----------|
| `scripts.external`           | CheckExternalScripts  | `files`: the scripts folder; `commands`: the names scripts are registered under |
| `scripts.python`             | PythonScript          | `files`: the Python scripts folder |
| `scripts.lua`                | LUAScript             | `files`: the Lua scripts folder |
| `scripts`                    | whichever are loaded  | one object with a key per loaded runtime |
| `pdh.objects`                | CheckSystem (Windows) | one record per performance object, with its `counters` |
| `pdh.instances.<object>`     | CheckSystem (Windows) | the instances that object has right now |

There is nothing to enable: an on-demand set is available while the module
that produces it is loaded, and costs nothing until it is read. What they
contain is on the [Host Facts](../../concepts/facts.md#on-demand-sets) page.

### Request

```
GET /api/v2/facts?path=scripts.python
```

A path below the set works as it does in the document
(`?path=scripts.external.files`).

### Response

The same envelope as the document, with `on_demand: true`:

```json
{
  "revision": 7,
  "collected": "2026-09-23T10:00:00Z",
  "path": "scripts.python",
  "found": true,
  "enabled": ["hardware", "os"],
  "errors": {},
  "gathered": {
    "hardware": "2026-09-23T06:12:41Z",
    "os": "2026-09-23T06:12:41Z",
    "scripts.python": "2026-09-23T10:41:07Z"
  },
  "facts": {
    "files": ["python/check_thing.py", "python/sample.py"]
  },
  "on_demand": true
}
```

`revision`, `collected` and `enabled` still describe the stored document: an
on-demand read does not move any of them. The set's own age is in `gathered`
and its collection error, if it had one, in `errors`, both under the id it is
listed with (`scripts.python`). `on_demand` is only ever present on these
answers, so every other response is byte for byte what it was.

### Refused with the document's grant

A caller holding `facts.get` but not `facts.get.scripts` gets a 403 that names
the missing grant, and **nothing is collected for it**:

```
403 The on-demand fact set 'scripts' requires the facts.get.scripts privilege
```

### A parameter

`pdh.instances` takes the object as the rest of the path, verbatim — a
performance object name has dots and spaces in it, and neither is path syntax
here:

```
GET /api/v2/facts?path=pdh.instances.Processor%20Information
```

```json
{ "path": "pdh.instances.Processor Information", "found": true, "on_demand": true,
  "facts": { "object": "Processor Information", "names": ["0,0", "0,1", "0,_Total", "_Total"] }, "…": "…" }
```

The parameter is at most 256 printable characters and is only ever compared
with the object names PDH itself enumerates; it is never parsed as options or
as a counter path. A name PDH does not have answers `found: false` with
`"errors": { "pdh.instances": "No such performance object" }`.

### A runtime that is not loaded

`?path=scripts.lua` without LUAScript loaded answers `found: false` and still
`on_demand: true` — the set exists, and nothing is producing that key.

### The cache

An answer is served again for **5 seconds**, so a page that reads the same set
from several places collects once, and a client cannot drive collection in a
loop. A collection that failed is not cached. `POST
/api/v2/facts/commands/refresh` forgets what is cached.

## List the sets

Which sets this agent has, without their contents.

| Key       | Value                  |
|-----------|------------------------|
| Verb      | GET                    |
| Address   | /api/v2/facts/sets     |
| Privilege | facts.get              |

### Response

```json
{
  "scheduled": ["hardware", "os"],
  "on_demand": [
    { "id": "pdh.instances", "parameterized": true, "privilege": "facts.get.pdh" },
    { "id": "pdh.objects", "parameterized": false, "privilege": "facts.get.pdh" },
    { "id": "scripts.external", "parameterized": false, "privilege": "facts.get.scripts" }
  ]
}
```

`scheduled` is the `enabled` list of the document. `on_demand` is what the
loaded modules can collect when asked, each with the grant that reads it.
Names only, which is why the document's grant is enough.

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
  button that drives the endpoint above, and an **On demand** card that reads
  an on-demand set when you ask for it.

## Endpoints that read facts

Two older endpoints answer from on-demand sets, with the bodies and grants
they always had:

| Endpoint | Reads | Grant (unchanged) |
|----------|-------|-------------------|
| `GET /api/v2/scripts/<runtime>` ([Scripts](scripts.md)) | `scripts.<runtime>` | `scripts.lists.<runtime>` |
| `GET /api/v2/metadata/counters` ([Metadata](metadata.md)) | `pdh.objects` | `metadata.get.counters` |

They used to run the module's command line (`list --json`, `pdh --list
--json`) and pass its output through; the listing is now read as data.
