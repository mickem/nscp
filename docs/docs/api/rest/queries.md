# Queries

The queries API can be used to list and execute queries (check commands)
provided by the loaded modules.

* [List queries](#list-queries)
* [Get query](#get-query)
* [Get query help](#get-query-help)
* [Executing without arguments](#executing-without-arguments)
* [Execute query](#command-execute)
* [Execute Query (Nagios format)](#command-execute_nagios)

The queries controller is mounted on both `/api/v1/queries` and
`/api/v2/queries`. The examples below use `/api/v2`, but `/api/v1` accepts
the same payloads.

## List queries

Returns a list of queries provided by the currently loaded modules. There
is no way to enumerate every possible query because external script runtimes
can register checks at any time.

| Key       | Value             |
|-----------|-------------------|
| Verb      | GET               |
| Address   | /api/v2/queries   |
| Privilege | queries.list      |

### Parameters

| Key | Value          | Description                                                |
|-----|----------------|------------------------------------------------------------|
| all | `true`/`false` | Include queries from modules that are not loaded yet.      |

### Request

```
GET /api/v2/queries
```

### Response

```json
[
    {
        "name": "check_cpu",
        "title": "check_cpu",
        "description": "Check that the load of the CPU(s) are within bounds.",
        "metadata": {},
        "query_url": "https://localhost:8443/api/v2/queries/check_cpu/"
    }
]
```

### Example

```
curl -s -k -u admin https://localhost:8443/api/v2/queries | python -m json.tool
```

## Get query

Returns details about a single query.

| Key       | Value                       |
|-----------|-----------------------------|
| Verb      | GET                         |
| Address   | /api/v2/queries/{query}     |
| Privilege | queries.get                 |

### Request

```
GET /api/v2/queries/check_cpu
```

### Response

```json
{
    "name": "check_cpu",
    "title": "check_cpu",
    "description": "Check that the load of the CPU(s) are within bounds.",
    "metadata": {},
    "execute_url":        "https://localhost:8443/api/v2/queries/check_cpu/commands/execute",
    "execute_nagios_url": "https://localhost:8443/api/v2/queries/check_cpu/commands/execute_nagios"
}
```

## Get query help

Returns the vocabulary of a single query: every option it accepts, with its
default value and description, and every filter keyword it offers. This is the
same information the interactive console prints for `desc` and `keywords`, and
what the web interface reads to highlight and complete an argument line.

| Key       | Value                        |
|-----------|------------------------------|
| Verb      | GET                          |
| Address   | /api/v2/queries/{query}/help |
| Privilege | queries.get                  |

### Request

```
GET /api/v2/queries/check_drivesize/help
```

### Response

```json
{
    "name": "check_drivesize",
    "keyword_source": "check_drivesize",
    "parameters": [
        {
            "name": "filter",
            "default_value": "none",
            "required": false,
            "repeatable": false,
            "content_type": "string",
            "short_description": "Filter which marks interesting items.",
            "long_description": "Filter which marks interesting items.
...
Common option for all filter checks."
        }
    ],
    "fields": [
        {
            "name": "free",
            "short_description": "",
            "long_description": "Free disk space"
        },
        {
            "name": "convert_bytes()",
            "short_description": "",
            "long_description": "Convert a byte value to another unit."
        }
    ]
}
```

A filter **function** is spelled with a trailing `()` on its name, which is how
the registry tells it from a variable; the suffix is not part of the name.

`content_type` is `bool` for an option that takes a boolean and `string` for
everything else. A boolean option still takes a value on the wire — checks
declare their flags so that REST can pass `show-all=true`, and a bare
`show-all` is refused with *does not take any arguments* — so `content_type` is
what tells a caller which of the two to send. A `bool` with an empty
`default_value` is a plain switch (`help`, `show-default`), which takes no
value at all.

`keyword_source` is the command the keywords belong to. It differs from `name`
only for an [alias](aliases.md), which declares no keywords of its own — its
filter expressions are written in the keywords of the command it stands for, so
that is the list answered with.

A query that is not filter based (it has options but no filter) answers with an
empty `fields` list. An unknown query is a `404`.

<!-- @formatter:off -->
!!! note "Common options are marked in the description, not in a field"
    An option or keyword shared by many checks carries a marker line at the end
    of its `long_description` — `Common option for all filter checks.`,
    `Common option for all commands.` or, for the generic summary keywords,
    `Common option for all checks.` The reference documentation and the web
    interface both split on those lines to keep the handful a check defines
    itself apart from the many every check has.
<!-- @formatter:on -->

## Commands

Two commands are available to execute a query. They differ only in the
shape of the result:

| Command          | Result format                                            |
|------------------|----------------------------------------------------------|
| `execute`        | Structured JSON with parsed performance data.            |
| `execute_nagios` | Plain Nagios-style payload (`message` + `perf` strings). |

## Executing without arguments

Two grants open the execute endpoints:

| Grant                    | May run a query | May pass arguments |
|--------------------------|-----------------|--------------------|
| `queries.execute`        | yes             | yes                |
| `queries.execute.noargs` | yes             | no                 |

`queries.execute.noargs` is the REST equivalent of the NRPE server's
`allow arguments = false`: the caller may run the checks the agent defines,
but cannot shape what they do. A request that carries any query-string
parameter is answered with `403 Arguments are not allowed for this user`
and the refusal is logged.

The built-in [`restricted`](../../setup/web-interface.md) role
(`public,queries.execute.noargs,aliases.list,login.get`) is exactly this.
Neither grant implies the other, so a `restricted` role can never widen into
the full privilege, and the existing `client` / `monitoring` roles keep
passing arguments as before. `full` (`*`) confers both.

To give such a caller a check that does need arguments, define an
[alias](aliases.md) — the arguments live in the agent's configuration and
the caller only names the alias:

```ini
[/settings/check helpers/alias]
check_root_disk = check_drivesize drive=/ warning=free<10% critical=free<5%
```

```
GET /api/v2/queries/check_root_disk/commands/execute
```

<!-- @formatter:off -->
!!! note "Credentials must travel in a header"
    Every query-string parameter counts as an argument, including a session
    token passed the legacy way as `?TOKEN=` — it is forwarded to the check
    like any other parameter, so exempting it would reopen the argument
    smuggling the grant exists to prevent. A no-arguments caller
    authenticates with the `Authorization`, `X-Auth-Token` or `TOKEN`
    header.
<!-- @formatter:on -->

## Command: execute

Executes a query and returns the result as structured JSON.

| Key       | Value                                                |
|-----------|------------------------------------------------------|
| Verb      | GET                                                  |
| Address   | /api/v2/queries/{query}/commands/execute             |
| Privilege | queries.execute (or queries.execute.noargs)          |

### Parameters

Any query-string parameter is forwarded as an argument to the check, e.g.
to configure `check_cpu` with three time windows:

```
GET /api/v2/queries/check_cpu/commands/execute?time=5m&time=30m&time=90m
```

A caller holding only `queries.execute.noargs` may not pass any — see
[Executing without arguments](#executing-without-arguments).

!!! note "Client-module commands and configured targets"
    The `submit_*` / `check_*` commands of the outbound client modules
    (NRDP, Icinga, SMTP, NSCA, NSCA-NG, NSCP) run against the module's
    configured `default` target, and that target's `password` or `token` is
    loaded with it. Moving the destination with `host`, `port` or `address`
    while that configured credential is the one that would travel is
    therefore refused — otherwise any holder of `queries.execute` could have
    the agent send those credentials to a host of their choosing. Supply the
    credential with the request, select another configured target with
    `target=`, or set `allow host override = true` on the target to permit
    the override explicitly. See
    [Security notices](../../security/notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host).

### Response

```json
{
    "command": "check_cpu",
    "result": 0,
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": {
                "total 3m": {
                    "value": 45,
                    "unit": "%",
                    "warning": 80,
                    "critical": 90,
                    "minimum": 0,
                    "maximum": 0
                }
            }
        }
    ]
}
```

`result` is the numeric Nagios state (`0`=OK, `1`=WARNING, `2`=CRITICAL,
`3`=UNKNOWN).

### Example

```
curl -s -k -u admin "https://localhost:8443/api/v2/queries/check_cpu/commands/execute?time=3m" \
    | python -m json.tool
```

## Command: execute_nagios

Executes a query and returns a Nagios-style payload.

| Key       | Value                                                |
|-----------|------------------------------------------------------|
| Verb      | GET                                                  |
| Address   | /api/v2/queries/{query}/commands/execute_nagios      |
| Privilege | queries.execute (or queries.execute.noargs)          |

### Parameters

Identical to [`execute`](#command-execute) — any query-string parameter is
forwarded to the check, and the same
[no-arguments restriction](#executing-without-arguments) applies.

### Response

```json
{
    "command": "check_cpu",
    "result": "OK",
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": "'total 3m'=41%;80;90"
        }
    ]
}
```

`result` is the textual Nagios state (`OK`, `WARNING`, `CRITICAL`,
`UNKNOWN`).

### Example

```
curl -s -k -u admin "https://localhost:8443/api/v2/queries/check_cpu/commands/execute_nagios?time=3m" \
    | python -m json.tool
```

