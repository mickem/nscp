# Scripts

The scripts API can be used to read view and modify the scripts which NSClient++ can run.

* [List modules](#list-modules)
* [Get module](#get-module)
* [Update module](#update-modules)
* [Load module](#load-module)
* [Unload module](#unload-module)

## List modules

The modules API will respond to get with a list of all currently loaded modules.
You can add `all=true` if you want to show modules which are not loaded as well.
This is significantly slower as NSClient++ has to inspect all available modules.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/modules
Privilege | modules.list

### Parameters

Key | Value        | Description
----|--------------|-----------------------------------------------------
all | true / false | If all scripts should be listed (not activated ones)

### Request

```
GET /api/v1/modules
```

### Response
```
[
    {
        "description": "Module used to execute external scripts",
        "id": "CheckExternalScripts",
        "loaded": true,
        "metadata": {
            "plugin_id": "0"
        },
        "name": "CheckExternalScripts",
        "title": "CheckExternalScripts"
    },
    {
        "description": "A server that listens for incoming ...",
        "id": "WEBServer",
        "loaded": true,
        "metadata": {
            "plugin_id": "1"
        },
        "name": "WEBServer",
        "title": "WEBServer"
    },
    {
        "description": "A command line client, generally not used except with \"nscp test\".",
        "id": "CommandClient",
        "loaded": true,
        "metadata": {
            "plugin_id": "2"
        },
        "name": "CommandClient",
        "title": "CommandClient"
    }
]
```

### Example

List all currently loaded modules.

```
curl -s -k -u admin https://localhost:8443/api/v1/modules |python -m json.tool
[
    {
        "description": "Module used to execute external scripts",
        "id": "CheckExternalScripts",
        "loaded": true,
        "metadata": {
            "plugin_id": "0"
        },
        "name": "CheckExternalScripts",
        "title": "CheckExternalScripts"
    },
    {
        "description": "A server that listens for incoming ...",
        "id": "WEBServer",
        "loaded": true,
        "metadata": {
            "plugin_id": "1"
        },
        "name": "WEBServer",
        "title": "WEBServer"
    },
    {
        "description": "A command line client, generally not used except with \"nscp test\".",
        "id": "CommandClient",
        "loaded": true,
        "metadata": {
            "plugin_id": "2"
        },
        "name": "CommandClient",
        "title": "CommandClient"
    }
]
```

## Get module

Get details about a given module.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/modules/:module
Privilege | modules.get

### Request

```
GET /api/v1/modules/:module
```

### Response

```
{
    "description": "A server that listens for ...",
    "id": "WEBServer",
    "loaded": true,
    "metadata": {
        "plugin_id": "1"
    },
    "name": "WEBServer",
    "title": "WEBServer"
}
```

### Example

Fetch details about the `WEBServer` module.

```
curl -s -k -u admin https://localhost:8443/api/v1/modules/WEBServer |python -m json.tool
{
    "description": "A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module.",
    "id": "WEBServer",
    "loaded": true,
    "metadata": {
        "plugin_id": "1"
    },
    "name": "WEBServer",
    "title": "WEBServer"
}
```

## Update module

As most module details are static the only thing which can be changed (currently) is the loaded flag.
This can be used to load/unload modules. To execute this command you need multiple privileges first we need modules.get to get a list of modules and then depending on what the desired action is you need either modules.load or modules.unload.

Key       | Value
----------|-----------------------------------------------
Verb      | PUT
Address   | /api/v1/modules/:module
Privilege | modules.get and modules.load or modules.unload

### Request

```
PUT /api/v1/modules/:module
```

### The posted payload

The payload we post is the same one we get from a `GET` except most attributed are ignored and can be left out:

```
{
    "loaded": true
}
```

### Response

```
Success unload CheckExternalScripts
```

### Example

An example of unloading the `CheckExternalScripts` module:

```
curl -s -k -u admin -X PUT https://localhost:8443/api/v1/modules/CheckExternalScripts -d "{\"loaded\":false}"
Success unload CheckExternalScripts
```

## Commands

In addition to REST full CRUD operation the module API also supports commands.

* Load module
* Unload modules

## Load Module

The load command is a convincing for doing a PUT on the module setting `loaded=true` to loading a module.

Key       | Value
----------|-----------------------------------------------
Verb      | GET
Address   | /api/v1/modules/:module/commands/load
Privilege | modules.get and modules.load

### Request

```
PUT /api/v1/modules/:module/commands/load
```

### Response

```
Success load CheckExternalScripts
```

### Example

An example of loading the `CheckExternalScripts` module:

```
curl -s -k -u admin https://localhost:8443/api/v1/modules/CheckExternalScripts/commands/load
Success load CheckExternalScripts
```

## Unload Module

The unload command is a convincing for doing a PUT on the module setting `loaded=false` to unload a module.

Key       | Value
----------|-----------------------------------------------
Verb      | GET
Address   | /api/v1/modules/:module/commands/unload
Privilege | modules.get and modules.unload

### Request

```
PUT /api/v1/modules/:module/commands/unload
```

### Response

```
Success unloaded CheckExternalScripts
```

### Example

An example of unloading the `CheckExternalScripts` module:

```
curl -s -k -u admin https://localhost:8443/api/v1/modules/CheckExternalScripts/commands/unload
Success unloading CheckExternalScripts
```
