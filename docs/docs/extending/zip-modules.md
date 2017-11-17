# Zip modules

Zip modules is a way to package a number of scripts and configuration inside a zip-file to make distributing them easier.
The Zip contains shell script or python scripts and some basic configuration instructions.

## Writing Zip modules.

At the heart of the zip-module is the `module.json` file which defines the module.

```
{
	"name": "Module Name",
	"description": "This is a description",
	"version": "1.0.0",
	"modules": [ "CheckSystem" ],
	"scripts": [
		{
			"provider": "CheckExternalScripts",
			"script": "scripts/check_ok.bat",
			"alias": "x",
			"command": "check_ok.bat \"Test Test\""
		},
		"scripts/python/sample.py"
	],
	"on_start": [
		"CheckSystem.add \"\\\\Fysisk disk(0 C:)\\\\Disktid i procent\""
	]
}
```

The zip `module.json` has the following sections:

Section     | Description
------------|-------------------------------------------------------------------------------------------------------
name        | The name of the module (Showed in for instance the Web Ui)
description | Description of module
version     | The version
modules     | A list of other modules this module require (they will be loaded on start but not added to the config)
scripts     | A list of script to add to NSClient++ (will be added on start but not to config)
on_start    | A list of commands to execute on start

### Modules

A list of modules you want to load inside NSClient++.
These modules will not be added to the config section of NSClient++ instead they are loaded when the module is loaded.
You can specify any module here including other zip-modules.

### Scripts

These scripts, like the modules, are never added to the config instead they are loaded when the module is loaded.
You can use two syntaxes either the shorthand where you only specify the name of the script and the module and alias will be derived from the script name and extension.

```
"scripts": [
	"scripts/python/sample.py"
],
```

Another way is to define an object where you can customize the details of the script.

Key      | Description
---------|------------------------------------------------------------------------
provider | The module which provides the functionality required to run this script.
script   | The script file (inside the zip) to load.
alias    | The alias of the script.
command  | The command to execute

```
"scripts": [
	{
		"provider": "CheckExternalScripts",
		"script": "scripts/check_ok.bat",
		"alias": "x",
		"command": "check_ok.bat \"Test Test\""
	},
],
```

### Loading zip modules

Zip-modules are placed inside the modules folder like other modules.
They can be loaded like normal modules:

```
[/settings]
CheckSystem = enabled
my-zip-module.zip = enabled
```

And all the APIs for regular modules work with zip-modules as well.
