# Scripts

The scripts API can be used to read view and modify the scripts which NSClient++ can run.

* [List runtimes](#list-runtimes)
* [List scripts](#list-scripts)
* [Fetch Script](#fetch-script)
* [Add script](#add-script)
* [Delete Script](#delete-script)

## Runtimes

As scripts can be provided by multiple plugins (LUAScripts, PythonScripts and CheckExternalScripts) there is a runtime selector which will send the information to the proper runtime.
Currently only external scripts are supported.

Key | Runtime              | Description                                                             | Status
----|----------------------|-------------------------------------------------------------------------|---------
ext | CheckExternalScripts | Any script which ix executed on command line                            | Complete
lua | LUAScripts           | Scripts written in the Lua language which is executed inside NSClient++ | Missing
py  | PythonScripts        | Scripts written in the Python language running inside NSClient++        | Complete


## Security

As a security mechanism only scripts residing in the configured `script root` folder is showed.

To configure the `script root` you can add the following to you configuration.

```
[/settings/external scripts]
script root=${scripts}
```

## List Runtimes

The API lists all available runtimes.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/scripts
Privilege | scripts.list.runtimes

### Request

```
GET /api/v1/scripts
```

### Response
```
[
  {
    "module":"CheckExternalScripts",
    "name":"ext",
    "title":"CheckExternalScripts"
  }
]
```

### Example

Fetch a list of all runtimes with curl

```
curl -s -k -u admin https://localhost:8443/api/v1/scripts |python -m json.tool
[
    {
        "ext_url": "https://localhost:8443/api/v1/scripts/ext",
        "module": "CheckExternalScripts",
        "name": "ext",
        "title": "CheckExternalScripts"
    }
]
```



## List Scripts

The API lists all available commands/scripts for a given runtime.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/scripts/:runtime
Privilege | scripts.lists.:runtime

### Parameters

Key | Value        | Description
----|--------------|-----------------------------------------------------
all | true / false | If all scripts should be listed (not activated ones)


### Request

```
GET /api/v1/scripts/ext
```

### Response

```
[
  'check_ok'
]
```

### Example 1: Listing active script

Fetch all active (currently enabled) scripts from `CheckExternalScripts`.

```
curl -s -k -u admin https://localhost:8443/api/v1/scripts/ext |python -m json.tool
[
    "check_ok"
]
```

### Example 2: Listing all scripts

#### Request

```
curl -s -k -u admin https://localhost:8443/api/v1/scripts/ext?all=true |python -m json.tool
[
    "scripts\\check_60s.bat",
    "scripts\\check_battery.vbs",
    "scripts\\check_files.vbs",
    "scripts\\check_long.bat",
    "scripts\\check_no_rdp.bat",
    "scripts\\check_ok.bat",
    "scripts\\check_ping.bat",
    "scripts\\check_printer.vbs",
    "scripts\\check_test.bat",
    "scripts\\check_test.ps1",
    "scripts\\check_test.vbs",
    "scripts\\check_updates.vbs",
    "scripts\\lua\\check_cpu_ex.lua",
    "scripts\\lua\\default_check_mk.lua",
    "scripts\\lua\\noperf.lua",
    "scripts\\lua\\test.lua",
    "scripts\\lua\\test_ext_script.lua",
    "scripts\\lua\\test_nrpe.lua",
    "scripts\\powershell.ps1"
]
```

## Fetch Script

Fetch the script definition (ext) and/or the actual script.

Key       | Value
----------|-----------------------------------
Verb      | GET
Address   | /api/v1/scripts/:runtime/:script
Privilege | scripts.get.:runtime

### Request

```
GET /api/v1/scripts/ext/check_ok
```

### Response

```
scripts\check_ok.bat "Everything will be fine"
```

### Example 1: Show command definitions

Show the commands definitions i.e. the configured command  which will be executed when the check is executed.

```
curl -s -k -u admin https://localhost:8443/api/v1/scripts/ext/check_ok
scripts\check_ok.bat "The world is always fine..."
```

### Example 2: Listing the actual script

Please note that since script definitions are really commands
there is no automated way to go from a script definition and its script.
But given the above definition we can discern that the script is called `scripts\check_ok.bat`.
We can use either `/` or `\` as path separator here.

```
curl -s -k -u admin https://localhost:8443/api/v1/scripts/ext/scripts/check_ok.bat
@echo OK: %1
@exit 0
```

## Add Script

Upload the new script definitions.
Please note that it is not possible to upload scripts to the same granularity as you can with the configuration.
For that you have to use the configuration API instead. This API is designed for convenience.
So for instance you cannot set arguments for scripts via this API.

Key       | Value
----------|-----------------------------------
Verb      | PUT
Address   | /api/v1/scripts/:runtime/:script
Privilege | scripts.add.:runtime

### Request

```
PUT /api/v1/scripts/ext/scripts\check_new.bat
```

### The posted payload

The payload we post is the actual script such as:

```
@echo OK: %1
@exit 0
```

### Response

```
Added check_new as scripts\check_new.bat
```

### Example

Given a file called `check_new.bat` which contains the following:

```
@echo OK: %1
@exit 0
```

We can use the following curl call to upload that as check_new.

```
curl -s -k -u admin -X PUT https://localhost:8443/api/v1/scripts/ext/scripts/check_new.bat --data-binary @check_new.bat
Added check_new as scripts\check_new.bat
```

### configuration

The configuration added to execute this script is:

```
[/settings/external scripts/scripts]

; SCRIPT - For more configuration options add a dedicated section (if you add a new section you can customize the user and various other advanced features)
check_new = scripts\check_new.bat
```

## Delete Script

Delete both script definitions and actual script files from disk.

Key       | Value
----------|-----------------------------------
Verb      | DELETE
Address   | /api/v1/scripts/:runtime/:script
Privilege | scripts.delete.:runtime

### Request

```
DELETE /api/v1/scripts/ext/scripts\check_new.bat
```

### Response

```
Script file was removed
```

### Example 1: Delete the script definition

If we have created a script for check_new (see adding script above) we can remove it via the API as well.
Please note this will **ONLY** remove the script definition not the actual script file (to remove the script see below).

```
curl -s -k -u admin -X DELETE https://localhost:8443/api/v1/scripts/ext/check_new
Script definition has been removed don't forget to delete any artifact for: scripts\check_new
```

### Example 2: Deleting the script file

To delete the script file we use the same trick as when we showed it above i.e. we specify the script file instead of the command name.

```
curl -s -k -u admin -X DELETE https://localhost:8443/api/v1/scripts/ext/scripts/check_new.bat
Script file was removed
```
