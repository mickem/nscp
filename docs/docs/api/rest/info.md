# Info

The info API can be used to retrieve information about the running NSClient++
instance such as its name and version.

* [Get information](#get)
* [Get version](#get-version)

The info controller is mounted on both `/api/v1/info` and `/api/v2/info`. The
examples below use `/api/v2`, but the response format is identical for both
versions.

## Get

Get information about NSClient++.

| Key       | Value         |
|-----------|---------------|
| Verb      | GET           |
| Address   | /api/v2/info  |
| Privilege | info.get      |

### Request

```
GET /api/v2/info
```

### Response

```json
{
    "name": "NSClient++",
    "version": "0.6.0 2026-04-29",
    "version_url": "https://localhost:8443/api/v2/info/version"
}
```

### Example

List information about NSClient++ with curl:

```
curl -s -k -u admin https://localhost:8443/api/v2/info | python -m json.tool
{
    "name": "NSClient++",
    "version": "0.6.0 2026-04-29",
    "version_url": "https://localhost:8443/api/v2/info/version"
}
```

## Get Version

Get only the version of NSClient++.

| Key       | Value                 |
|-----------|-----------------------|
| Verb      | GET                   |
| Address   | /api/v2/info/version  |
| Privilege | info.get.version      |

### Request

```
GET /api/v2/info/version
```

### Response

```json
{
    "version": "0.6.0 2026-04-29"
}
```

### Example

Get the version of NSClient++ with curl:

```
curl -s -k -u admin https://localhost:8443/api/v2/info/version | python -m json.tool
{
    "version": "0.6.0 2026-04-29"
}
```

