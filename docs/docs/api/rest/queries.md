# Queries

The query API can be used to list and execute queries. Queries are check commands which other modules provide.

* [List queries](#list-queries)
* [Get query](#get-query)
* [Execute query](#command-execute)
* [Execute Query (Nagios)](#command-execute_nagios)

## List queries

The default `GET` operation on the query API is to fetch a list of all available (currently loaded) modules.
There is no way to get a list of all possible queries as that is an unknown set.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/queries
Privilege | queries.list

### Request

```
GET /api/v1/queries
```

### Response
```
[
    {
        "description": "Check that the load of the CPU(s) are within bounds.\n\nThe check_cpu command is a query based command which means it has a filter where you can use a filter expression with filter keywords to define which rows are relevant to the check.\nThe filter is written using the filter query language and in it you can use various filter keywords to define the filtering logic.\nThe filter keywords can also be used to create the bound expressions for the warning and critical which defines when a check returns warning or critical.",
        "metadata": {},
        "name": "check_cpu",
        "query_url": "https://localhost:8443/api/v1/queries/check_cpu/",
        "title": "check_cpu"
    },
    ...
]
```

### Example

List all currently loaded queries.

```
curl -s -k -u admin https://localhost:8443/api/v1/queries |python -m json.tool
[
    {
        "description": "Check that the load of the CPU(s) are within bounds....",
        "metadata": {},
        "name": "check_cpu",
        "query_url": "https://localhost:8443/api/v1/queries/check_cpu/",
        "title": "check_cpu"
    },
    ...
]
```

## Get query

Get details about a given query.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/queries/:query
Privilege | queries.get

### Request

```
GET /api/v1/queries/:query
```

### Response

```
{
    "description": "Check that the load of the CPU(s) are within bounds....",
    "execute_nagios_url": "https://localhost:8443/api/v1/queries/check_cpu/commands/execute_nagios",
    "execute_url": "https://localhost:8443/api/v1/queries/check_cpu/commands/execute",
    "metadata": {},
    "name": "check_cpu",
    "title": "check_cpu"
}
```

### Example

Fetch details about the `check_cpu` query.

```
curl -s -k -u admin https://localhost:8443/api/v1/queries/check_cpu |python -m json.tool
{
    "description": "Check that the load of the CPU(s) are within bounds....",
    "execute_nagios_url": "https://localhost:8443/api/v1/queries/check_cpu/commands/execute_nagios",
    "execute_url": "https://localhost:8443/api/v1/queries/check_cpu/commands/execute",
    "metadata": {},
    "name": "check_cpu",
    "title": "check_cpu"
}
```

## Commands

In addition to REST full CRUD operation the module API also supports commands.
The difference between the two commands are the format of the result of the query.

* execute
* execute_nagios

## Command: execute

Executes a query and returns the result.

Key       | Value
----------|-----------------------------------------------
Verb      | GET
Address   | /api/v1/queries/:query/commands/execute
Privilege | queries.execute

### Parameters

Any parameter will be passed along to the script so for instance to configure the time for `check_cpu` you would:

```
GET /api/v1/queries/check_cpu/commands/execute?time=5m\&time=30m\&time=90m
```

### Request

```
GET /api/v1/queries/:query/commands/execute
```

### Response

```
{
    "command": "check_cpu",
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": {
                "total 3m": {
                    "critical": 90,
                    "maximum": 0,
                    "minimum": 0,
                    "unit": "%",
                    "value": 45,
                    "warning": 80
                }
            }
        }
    ],
    "result": 0
}
```

### Example

An example of executing check_cpu with a 3 minute time interval:

```
curl -s -k -u admin "https://localhost:8443/api/v1/queries/check_cpu/commands/execute?time=3m" | python -m json.tool
{
    "command": "check_cpu",
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": {
                "total 3m": {
                    "critical": 90,
                    "maximum": 0,
                    "minimum": 0,
                    "unit": "%",
                    "value": 45,
                    "warning": 80
                }
            }
        }
    ],
    "result": 0
}
```

## Command: execute_nagios

Used to execute a command but return the data in an easy to read nagios format.

Key       | Value
----------|-----------------------------------------------
Verb      | GET
Address   | /api/v1/queries/:query/commands/execute_nagios
Privilege | queries.execute

### Parameters

Any parameter will be passed along to the script so for instance to configure the time for `check_cpu` you would:

```
GET /api/v1/queries/check_cpu/commands/execute_nagios?time=5m\&time=30m\&time=90m
```

### Request

```
GET /api/v1/queries/:query/commands/execute_nagios
```

### Response

```
{
    "command": "check_cpu",
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": "'total 3m'=41%;80;90"
        }
    ],
    "result": "OK"
}
```

### Example

An example of excuting check_cpu with a 3 minutes time interval:

```
curl -s -k -u admin "https://localhost:8443/api/v1/queries/check_cpu/commands/execute_nagios?time=3m" | python -m json.tool
{
    "command": "check_cpu",
    "lines": [
        {
            "message": "OK: CPU load is ok.",
            "perf": "'total 3m'=41%;80;90"
        }
    ],
    "result": "OK"
}
```
