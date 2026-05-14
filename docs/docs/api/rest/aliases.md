# Aliases

The aliases API lists the registered query aliases — admin-defined wrappers
around real check commands. Aliases are configured under
`[/settings/check helpers/alias]` (handled by `CheckHelpers`) or
`[/settings/external scripts/alias]` (handled by `CheckExternalScripts`)
and surface as `QUERY_ALIAS` items in the registry.

* [List aliases](#list-aliases)

The aliases controller is mounted on both `/api/v1/aliases` and
`/api/v2/aliases`. The examples below use `/api/v2`, but `/api/v1` accepts
the same payloads.

> **Note**
>
> There is no separate execute endpoint on `/aliases` — an alias is
> dispatched by the core as if it were a regular query. To run one, follow
> the `query_url` returned in the list response (it points at
> `/api/vX/queries/<alias>/`) and use the standard
> [queries execute](queries.md#command-execute) flow.

## List aliases

Returns a list of every alias registered with the loaded modules.

| Key       | Value             |
|-----------|-------------------|
| Verb      | GET               |
| Address   | /api/v2/aliases   |
| Privilege | aliases.list      |

### Parameters

| Key | Value          | Description                                                |
|-----|----------------|------------------------------------------------------------|
| all | `true`/`false` | Include aliases from modules that are not loaded yet.      |

### Request

```
GET /api/v2/aliases
```

### Response

```json
[
    {
        "name": "mock_alias",
        "title": "mock_alias",
        "description": "Alias for: mock_query",
        "plugin": "CheckHelpers",
        "metadata": {},
        "query_url": "https://localhost:8443/api/v2/queries/mock_alias/"
    }
]
```

The `query_url` field intentionally points at the queries endpoint rather
than back at `/aliases/<name>` — clients execute the alias via the regular
queries flow.

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/aliases | python -m json.tool
```

## Required privileges

The bundled roles grant `aliases.list` to:

* `client` — read-only role used by the web UI.
* `monitoring` — minimal grant for clients that only run checks.
* `full` — `*` (everything).

A custom role needs the `aliases.list` privilege to call this endpoint:

```
[/settings/WEB/server/roles]
my_role=aliases.list
```
