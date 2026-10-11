# Settings API

The settings endpoints are mounted under `/api/v2/settings` (and the legacy
`/api/v1/settings`) and let you read, update, and remove configuration
values, plus drive maintenance operations such as `save`, `load`, `reload`,
`status`, `diff`, and `problems`.

## Required grants

| Operation                                          | Grant              |
|----------------------------------------------------|--------------------|
| `GET /…` — read keys, descriptions, status, diff, problems | `settings.get` |
| `PUT /…` — create or update keys                   | `settings.put`     |
| `POST /command` — `save` / `load` / `reload`       | `settings.put`     |
| `DELETE /…` — remove keys or paths                 | `settings.delete`  |

`settings.delete` is intentionally separate from `settings.put` — granting
write access does not automatically grant the ability to remove keys.

## Read

### List a path

```
GET /api/v2/settings/{path}
```

Returns every key defined under `{path}` (recursively), with the current
value of each key.

```json
[
  { "path": "/settings/default", "key": "password", "value": "secret" },
  { "path": "/settings/default", "key": "allowed hosts", "value": "127.0.0.1" }
]
```

### Describe a path

```
GET /api/v2/settings/descriptions/{path}?recursive=true&samples=false
```

Returns the same nodes plus their static metadata (title, description,
`type`, `default_value`, whether the key is advanced, sample, template,
sub-key, etc.) merged with the current value.

## Write

### Update a single key

```
PUT /api/v2/settings/{path}
Content-Type: application/json

{
  "path": "/settings/default",
  "key":  "password",
  "value": "new-secret"
}
```

If the body's `path` field is empty the URL `{path}` is used. The `key`
field is required. A response of `{"status":"success","keys":1}` means one
update was queued.

> **Note:** writes are queued in memory. Call
> `POST /api/v2/settings/command` with `{"command": "save"}` to persist
> them, or `{"command": "reload"}` to restart the service so it picks up
> the new values.

### Update multiple keys at once

The request body may also be an array — every entry is processed in order
inside a single settings transaction:

```json
[
  { "path": "/settings/NRPE/server", "key": "port",        "value": "5666" },
  { "path": "/settings/NRPE/server", "key": "ssl options", "value": "no-sslv3" }
]
```

## Delete

`DELETE` removes keys and paths. The handler accepts three request shapes:

### Form A — query-string

```
DELETE /api/v2/settings/{path}?key={name}
```

Removes the single key `{name}` under `{path}`.

```
DELETE /api/v2/settings/{path}
```

When neither `?key=` nor a body is supplied, **the whole path is removed**.
This is intentional — `settings.delete` callers asking for the path with no
qualifier are expected to mean "remove this section entirely".

A request with no path **and** no key is rejected with `400 Bad Request` —
this avoids accidentally asking the core to drop the root because a client
sent a trailing-slash URL.

### Form B — single object body

```
DELETE /api/v2/settings/{path}
Content-Type: application/json

{ "key": "password" }
```

Same effect as Form A but lets you point at a different `path` than the
URL:

```json
{ "path": "/settings/default", "key": "password" }
```

### Form C — array body (batch)

```json
[
  { "path": "/settings/NRPE/server", "key": "port" },
  { "path": "/settings/NRPE/server", "key": "ssl options" },
  { "path": "/settings/NSCA/server" }
]
```

Each entry is removed independently. An entry with a non-empty `key`
removes that key; an entry with no `key` removes its `path`.

The response mirrors `PUT`:

```json
{ "status": "success", "keys": 3 }
```

## Maintenance commands

```
POST /api/v2/settings/command
Content-Type: application/json

{ "command": "save" }     // persist the in-memory store to disk
{ "command": "load" }     // re-read the on-disk store
{ "command": "reload" }   // ask the running service to reload (delayed)
```

## Status and diff

```
GET /api/v2/settings/status
```

```json
{ "context": "ini://${shared-path}/nsclient.ini", "type": "ini", "has_changed": true }
```

```
GET /api/v2/settings/diff?path=/settings/default&recursive=true
```

Returns every change made since the last `save`, classified as
`added`, `removed`, `modified`, `path_added`, or `path_removed`. Sensitive
keys have their `old_value` / `new_value` redacted to `***`.

## Problems

```
GET /api/v2/settings/problems
```

Compares the saved configuration (the active store and everything it
includes) against the settings the loaded modules registered, and lists the
sections and keys nothing reads. It is the report `nscp settings --validate`
prints and the agent logs at startup, see
[Checking the configuration](../../concepts/settings.md#checking-the-configuration).

```json
{
  "problems": [
    {
      "kind": "relative_path",
      "severity": "warning",
      "source": "/etc/nsclient/nsclient.ini",
      "path": "settings/external scripts/scripts",
      "key": "",
      "suggestion": "/settings/external scripts/scripts",
      "message": "Section [settings/external scripts/scripts] is ignored: settings paths start with '/'. Did you mean [/settings/external scripts/scripts]?"
    }
  ],
  "count": 1,
  "warnings": 1
}
```

| `kind`          | `severity` | Meaning |
|-----------------|------------|---------|
| `orphan_key`    | warning    | A key above the first `[section]` header. |
| `relative_path` | warning¹   | A section whose name does not start with `/`. `suggestion` is the name with the slash. |
| `unknown_path`  | warning¹   | A section no loaded module uses, a typo away from one that a loaded module does use (`suggestion`). |
| `unknown_key`   | warning    | A key no loaded module uses, a typo away from one in the same section (`suggestion`). |
| `unused_path`   | info       | A section no loaded module uses. It may belong to a module that is not enabled. |
| `unused_key`    | info       | A key no loaded module uses, in a section that one does. It may belong to a module that is not enabled. |

¹ `info` when the section is empty: it configures nothing, so nothing in it is
ignored.

`warnings` counts the `warning` entries; the web UI raises its banner when it
is not zero. Unsaved changes are not part of the report: it describes what the
agent reads at its next start.
