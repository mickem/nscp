# Queries

The queries API can be used to list and execute queries (check commands)
provided by the loaded modules.

* [List queries](#list-queries)
* [Get query](#get-query)
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

## Commands

Two commands are available to execute a query. They differ only in the
shape of the result:

| Command          | Result format                                            |
|------------------|----------------------------------------------------------|
| `execute`        | Structured JSON with parsed performance data.            |
| `execute_nagios` | Plain Nagios-style payload (`message` + `perf` strings). |

## Command: execute

Executes a query and returns the result as structured JSON.

| Key       | Value                                                |
|-----------|------------------------------------------------------|
| Verb      | GET                                                  |
| Address   | /api/v2/queries/{query}/commands/execute             |
| Privilege | queries.execute                                      |

### Parameters

Any query-string parameter is forwarded as an argument to the check, e.g.
to configure `check_cpu` with three time windows:

```
GET /api/v2/queries/check_cpu/commands/execute?time=5m&time=30m&time=90m
```

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
| Privilege | queries.execute                                      |

### Parameters

Identical to [`execute`](#command-execute) — any query-string parameter is
forwarded to the check.

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

