# Logs

The logs API can be used to read and write log entries.

* [List log entries](#list)
* [Add log entry](#add)
* [List logs since](#list-since)
* [Get status](#status)
* [Reset status](#reset-status)
* [Delete logs](#delete)

## List

The API lists log records.

| Key     | Value        |
|---------|--------------|
| Verb    | GET          |
| Address | /api/v2/logs |

### Parameters

| Key      | Value | Description              |
|----------|-------|--------------------------|
| page     | 1     | The page to read         |
| per_page | 100   | Number of items per page |

### Request

```
GET /api/v2/logs?page=1&per_page=100
```

### Response
```json
[
    {
        "date": "2017-Nov-08 22:06:46",
        "file": "rest.test.ts",
        "level": "info",
        "line": 123,
        "message": "001: This is a test log message from automated tests"
    }
]
```

## Add

Add a new log entry.

| Key     | Value        |
|---------|--------------|
| Verb    | POST         |
| Address | /api/v2/logs |

### Request

```json
{
    "level": "info",
    "message": "This is a test log message",
    "file": "rest.test.ts",
    "line": 123
}
```

### Response
```json
""
```

## List Since

Fetch logs since a specific index.

| Key     | Value              |
|---------|--------------------|
| Verb    | GET                |
| Address | /api/v2/logs/since |

### Parameters

| Key      | Value | Description                                                 |
|----------|-------|-------------------------------------------------------------|
| page     | 1     | The page to read                                            |
| per_page | 100   | Number of items per page                                    |
| since    | 12345 | The index to read from (obtained from `X-Log-Index` header) |

### ResponseHeaders

| Key         | Description           |
|-------------|-----------------------|
| x-log-index | The current log index |

### Response
```json
[
    {
        "date": "2017-Nov-08 22:06:46",
        "file": "rest.test.ts",
        "level": "error",
        "line": 456,
        "message": "002: This is a test error log message from automated tests"
    }
]
```

## Status

Get the current log status.

| Key     | Value               |
|---------|---------------------|
| Verb    | GET                 |
| Address | /api/v2/logs/status |

### Response
```json
{
    "errors": 0,
    "last_error": ""
}
```

## Reset Status

Reset the log status.

| Key     | Value               |
|---------|---------------------|
| Verb    | DELETE              |
| Address | /api/v2/logs/status |

### Response
```json
""
```

## Delete

Delete all cached log entries.

| Key     | Value        |
|---------|--------------|
| Verb    | DELETE       |
| Address | /api/v2/logs |

### Response
```json
{
    "count": 0
}
```
