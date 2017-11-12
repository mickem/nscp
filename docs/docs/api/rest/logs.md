# Scripts

The scripts API can be used to read view and modify the scripts which NSClient++ can run.

* [List log entries](#list)

## List

The API lists log records.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/logs
Privilege | logs.list.runtimes

### Parameters

Key   | Value       | Description
------|-------------|-----------------------------------------------------
pos   | 23          | The position in the log to read from (record number)
level | error,debug | Filter out only certain log levels

### Request

```
GET /api/v1/logs
```

### Response
```
{
    "log": {
        "data": [
            {
                "date": "2017-Nov-08 22:06:46",
                "file": "C:\\source\\nscp\\modules\\WEBServer\\WEBServer.cpp",
                "level": "error",
                "line": 183,
                "message": "No password set please run nscp web --help"
            }
        ],
        "pos": 23
    }
}
```

### Example

Fetch a list of all runtimes with curl

```
curl -s -k -u admin https://localhost:8443/api/v1/logs?level=error |python -m json.tool
{
    "log": {
        "data": [
            {
                "date": "2017-Nov-08 22:06:46",
                "file": "C:\\source\\nscp\\modules\\WEBServer\\WEBServer.cpp",
                "level": "error",
                "line": 183,
                "message": "No password set please run nscp web --help"
            }
        ],
        "pos": 23
    }
}
```
