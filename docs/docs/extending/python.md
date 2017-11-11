# Python Scripts

In addition to executing Python script as regular script NSClient++ can also execute them via the PythonScript module inside NSClient++.
The benefit of this is that you can call APIs of NSClient++ as well as maintain state.

To add a python script as an internal script you need to load the PythonScript module as well as configure it to run your script:

There is a command line interface to do this:

```
nscp py add --script my_script.py
```

This will create the following configuration for you:

```
[/modules]
PythonScript = enabled

[/settings/python/scripts]
my_script = my_script.py
```

## Functions

The skeleton of a script is the following three functions:

 - __main__
 - init
 - shutdown

### __main__

The mainfunction is used when the script is run from the command line.
For instance when you want to provide an install or helper function to the administrator much like we did when we installed the script:

```
nscp py execute --script my_script.py install --root /tmp
```

The main function takes as input argument a list of command line options:

```
from NSCP import log
def __main__(args):
    log("My arguments are: %s"%args)
```

Running this script:

```
nscp py execute --script my_script.py install --root /tmp
L     python My arguments are: ['install', '--root', '/tmp']
```

### init

The init function called when the script is loaded and provides important variables for talking to NSClient++.
The function takes tree arguments:

- plugin_id
- plugin_alias
- script_alias

The plugin id is an internal id of the plugin instance which is used in many API calls.
The plugin and script aliases can be used to differentiate between which instance is loaded.
The idea is that you can load a script multiple times in multiple modules and they should then have seprate config.

```
from NSCP import log
def init(plugin_id, plugin_alias, script_alias):
    log("*** plugin_id: %d, plugin_alias: %s, script_alias: %s"%(plugin_id, plugin_alias, script_alias))
```

The result of running this script from NSClient++:
```
...
D     python boot python
D     python Prepare python
D     python init python
D     python Adding script: osmc (c:\source\build\x64\dev\scripts\python\osmc.py)
D     python Loading python script: c:\source\build\x64\dev\scripts\python\osmc.py
D     python Lib path: c:\source\build\x64\dev\scripts\python\lib
L     python *** plugin_id: 0, plugin_alias: python, script_alias: my_script
...
```

### plugin_alias versus script_alias

Consider the following example:

```
[/modules]
ps1 = PythonScript
ps2 = PythonScript

[/settings/ps1/scripts]
ms1 = my_script.py
ms2 = my_script.py

[/settings/ps2/scripts]
ms1 = my_script.py
ms2 = my_script.py

```

This will load the same script four times with the following arguments:

plugin_id | plugin_alias | script_alias
----------|--------------|-------------
1         | ps1          | ms1
2         | ps1          | ms2
3         | ps2          | ms1
4         | ps2          | ms2

Thus when loading configuration it is recomended to place it under the following pseudo key `/settings/:script_name/:plugin_alias/:script_alias/` to allow for users loading a script multiple times.

At the same time it is discouraged from using the alias concept to load a script multipel times which is why the command line interfaces does not provide commands for doing so.
But in complicated sceraios it can be very usefull but it causes a lot of complexity.

### shutdown

The shutdown function is called whenver NSClient++ is shutting down or the PythonScript odule is unloaded.
The function takes no arguments.

```
from NSCP import log
def shutdown(args):
    log("Good bye")
```

## API

NSClient++ provides a rich API where you can do just about anything that can be done with NSCLient++.
THis includes loading modules, accessing settings, provind command line interfaces etc etc.
While most of it has a simple wrappers which you can use directly some things might require you to use the underlaying protobuf Api.

The API is split three modules.
 - Settings
 - Registry
 - Core

There is also an enum: `status` as well as a some direct functions:
 - log
 - log_error
 - log_debug
 - sleep
