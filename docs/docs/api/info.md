# Scripts

The scripts API can be used to read view and modify the scripts which NSClient++ can run.

* [Get information](#get)
* [Get version](#get-version)

## Get

Get information about NSClient++.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/info
Privilege | info.get

### Request

```
GET /api/v1/info
```

### Response
```
{
    "name": "NSClient++",
    "version": "0.5.2.13 2017-11-06"
}
```

### Example

List information about NSClient++ with curl

```
curl -s -k -u admin https://localhost:8443/api/v1/info |python -m json.tool
{
    "name": "NSClient++",
    "version": "0.5.2.13 2017-11-06"
}
```

## Get Version

Get the version of NSClient++.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/info/version
Privilege | info.get.version

### Request

```
GET /api/v1/info/version
```

### Response
```
{
    "version": "0.5.2.13 2017-11-06"
}
```

### Example

Get the version of NSClient++ with curl

```
curl -s -k -u admin https://localhost:8443/api/v1/info/version |python -m json.tool
{
    "version": "0.5.2.13 2017-11-06"
}
```
