# Zip modules

Zip modules is a way to packge a number of scripts and configuration inside a zip-file to make distributin easier.
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
