# Modules

The modules API can be used to list, load, unload, enable and disable the
plugin modules NSClient++ knows about.

* [List modules](#list-modules)
* [Get module](#get-module)
* [Update module](#update-module)
* [Commands: load, unload, enable, disable](#commands)

The modules controller is mounted on both `/api/v1/modules` and
`/api/v2/modules`. The examples below use `/api/v2`, but `/api/v1` accepts
the same payloads.

## Module status fields

NSClient++ distinguishes two flags on every module:

| Field     | Meaning                                                                 |
|-----------|-------------------------------------------------------------------------|
| `loaded`  | The shared library is loaded into the running process.                  |
| `enabled` | The module is configured to load on startup (`<Module> = enabled`).     |

`loaded` and `enabled` are independent — you can temporarily unload a module
without disabling it (it will come back on the next start), and you can mark
a module enabled in the configuration without forcing it to load right now.

## List modules

Returns all currently loaded modules. Pass `?all=true` to also include
modules that are not currently loaded — this is significantly slower because
NSClient++ has to inspect every module on disk.

| Key       | Value             |
|-----------|-------------------|
| Verb      | GET               |
| Address   | /api/v2/modules   |
| Privilege | modules.list      |

### Parameters

| Key | Value          | Description                                                |
|-----|----------------|------------------------------------------------------------|
| all | `true`/`false` | Include modules that are not currently loaded (default `false`). |

### Request

```
GET /api/v2/modules
```

### Response

```json
[
    {
        "id": "CheckExternalScripts",
        "name": "CheckExternalScripts",
        "title": "CheckExternalScripts",
        "description": "Module used to execute external scripts",
        "loaded": true,
        "enabled": true,
        "metadata": { "plugin_id": "0" },
        "load_url":    "https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/load",
        "unload_url":  "https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/unload",
        "enable_url":  "https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/enable",
        "disable_url": "https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/disable"
    }
]
```

### Example

List currently loaded modules:

```
curl -s -k -u admin https://localhost:8443/api/v2/modules | python -m json.tool
```

List every module known to NSClient++ (loaded or not):

```
curl -s -k -u admin "https://localhost:8443/api/v2/modules?all=true" | python -m json.tool
```

## Get module

Returns details about a single module.

| Key       | Value                       |
|-----------|-----------------------------|
| Verb      | GET                         |
| Address   | /api/v2/modules/{module}    |
| Privilege | modules.get                 |

### Request

```
GET /api/v2/modules/WEBServer
```

### Response

```json
{
    "id": "WEBServer",
    "name": "WEBServer",
    "title": "WEBServer",
    "description": "A server that listens for incoming HTTP connections...",
    "loaded": true,
    "enabled": true,
    "metadata": { "plugin_id": "1" },
    "load_url":    "https://localhost:8443/api/v2/modules/WEBServer/commands/load",
    "unload_url":  "https://localhost:8443/api/v2/modules/WEBServer/commands/unload",
    "enable_url":  "https://localhost:8443/api/v2/modules/WEBServer/commands/enable",
    "disable_url": "https://localhost:8443/api/v2/modules/WEBServer/commands/disable"
}
```

## Update module

Replace the runtime state of a module. Only the `loaded` flag is honoured;
all other fields are ignored. To execute this command the caller must have
`modules.get` plus either `modules.load` or `modules.unload`.

| Key       | Value                                                |
|-----------|------------------------------------------------------|
| Verb      | PUT (or POST)                                        |
| Address   | /api/v2/modules/{module}                             |
| Privilege | modules.get and modules.load or modules.unload       |

### Request

```
PUT /api/v2/modules/CheckExternalScripts
Content-Type: application/json

{
    "loaded": true
}
```

### Response

A short textual confirmation, for example:

```
Success unload CheckExternalScripts
```

### Example

```
curl -s -k -u admin -X PUT \
  https://localhost:8443/api/v2/modules/CheckExternalScripts \
  -d '{"loaded":false}'
```

## Commands

In addition to the REST CRUD operations, the modules API exposes four
convenience commands:

| Command   | Effect                                              | Privilege         |
|-----------|-----------------------------------------------------|-------------------|
| `load`    | Load the shared library into the running process.   | `modules.load`    |
| `unload`  | Unload the shared library from the running process. | `modules.unload`  |
| `enable`  | Mark the module as `enabled` in the configuration.  | `modules.enable`  |
| `disable` | Mark the module as `disabled` in the configuration. | `modules.disable` |

All four use the same shape:

| Key       | Value                                                 |
|-----------|-------------------------------------------------------|
| Verb      | GET                                                   |
| Address   | /api/v2/modules/{module}/commands/{command}           |

`enable` and `disable` only change the configuration — they do not
load/unload the module immediately. If you want the configuration change
applied right away you also need to `load` or `unload` the module (or
reload the service via the [Settings](settings.md) API).

### Request

```
GET /api/v2/modules/CheckExternalScripts/commands/load
```

### Response

```
Success load CheckExternalScripts
```

### Examples

```
curl -s -k -u admin https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/load
curl -s -k -u admin https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/unload
curl -s -k -u admin https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/enable
curl -s -k -u admin https://localhost:8443/api/v2/modules/CheckExternalScripts/commands/disable
```

Unknown command names return `404 Not Found` with a body of
`unknown command: <name>`.

