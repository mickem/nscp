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
py  | PythonScripts        | Scripts written in the Python language running inside NSClient++        | Missing


## Security

As a security mechanism only scripts residing in the configured `script root` folder is showed.

To configure the `script root` you can add the following to you configuration.

```
[/settings/external scripts]
script root=${scripts}
```

## List Runtimes

The API lists all avalible runtimes.

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

## List Scripts

The API lists all avalible commands/scripts for a given runtime.

Key       | Value
----------|----------------------
Verb      | GET
Address   | /api/v1/scripts/<runtime>
Privilege | scripts.lists.<runtime>

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

### Listing all scripts

#### Request

```
GET /api/v1/scripts/ext?all=true
```

#### Response

```
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
Address   | /api/v1/scripts/<runtime>/<script>
Privilege | scripts.get.<runtime>

### Request

```
GET /api/v1/scripts/ext/check_ok
```

### Response

```
scripts\check_ok.bat "Everything will be fine"
```

### Listing the actual script

Please note that since script definitions are really commands
there is no automated way to go from a script definition and its script.

#### Request

```
GET /api/v1/scripts/ext/scripts\check_ok.bat
```

#### Response

```
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
Address   | /api/v1/scripts/<runtime>/<script>
Privilege | scripts.add.<runtime>

### Request

```
PUT /api/v1/scripts/ext/scripts\check_new.bat
```

### Example
```
@echo OK: %1
@exit 0
```

### Response

```
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

Delete both script defenitions and actual script files from disk.

Key       | Value
----------|-----------------------------------
Verb      | DELETE
Address   | /api/v1/scripts/<runtime>/<script>
Privilege | scripts.delete.<runtime>

### Request

```
DELETE /api/v1/scripts/ext/scripts\check_new.bat
```

### Response

```
Script file was removed
```

### Deleting a script defintion

You can also via the same API delete any script definitions.

#### Request

```
GET /api/v1/scripts/ext/check_ok
```

#### Response

```
Script definition has been removed don't forget to delete any artifact for: scripts\check_new.bat
```
