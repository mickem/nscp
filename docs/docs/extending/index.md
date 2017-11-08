# Extending NSClient++

NSClient++ can be extended in multiple ways.
The easy choice is adding external scripts which are simply scripts which are executed as is by NSClient++ but if you do not fany easy choices you can also go hard-core and write your own custom Moules in C++. Here I will list the various options and some simple pros and cons.


## External scripts

External scripts are script you write in any language of choice.
They are executed exactly like regular commands on the host operating system.
The script has to conform to the nagios plugin guidelines.

**Pros:**

- Very easy to write your own script in your favorite scripting language.
- Powerfull in that your script can do whatever you want

**Cons:**

- Staeless, unless you implement the state your self.
- Forking overhead as the script is started afress each time it is executed
- No background threads.
- Only supported by check_commands

## Lua Scripts

Script written in the Lua scripting language.
These script run inside NSClient++ and can interact with NSClient++.

**Pros:**

- Very Powerfull in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, querys, passive results as well as events.
- Statefull as the scripts run in the background
- No forking overhead

**Cons:**

- Lua might be cinsiderd a slightly arcane language in the monitor space.
- Adding third party modules requires compiling modules.
- No background threads.

## Python Scripts

Script written in the Python scripting language.
These script run inside NSClient++ and can interact with NSClient++.

**Pros:**

- Very Powerfull in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, querys, passive results as well as events.
- Statefull as the scripts run in the background
- No forking overhead
- Background threads.

**Cons:**

- Slightly akward to distribute third party modules.

## .Net modules

Just as we can write modules in C++ we can also write them in any language supported by the dot-net runtime sich as C#, Haskel and VisualBasic.
These modules run inside NSClient++ and can interact with NSClient++.

**Pros:**

- Very Powerfull in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, querys, passive results as well as events.
- Statefull as the scripts run in the background
- No forking overhead
- Background threads.

**Cons:**

- Requires you to compile the modules meaning modifications are hard.

## C/C++ modules

This is how al lthe built-in functionality is written.

**Pros:**

- Very Powerfull in that your script can do whatever you want and you can interact with NSClient++.
- Supports all features of NSclient++, metrics, commands, querys, passive results as well as events.
- Statefull as the scripts run in the background
- No forking overhead
- Background threads.

**Cons:**

- Requires you to compile the modules meaning modifications are hard.


## Zip modules

This is new in 0.5.2 and a way to package scripts and configuration inside a zip file fo easy distribution in NSClient++.

**Pros:**

- Powerful: While not scripting it allows you to add both External Scrtips and python scrtips.
- Supports all features of NSclient++, metrics, commands, querys, passive results as well as events.
- Statefull as the scripts run in the background
- No forking overhead (dpending on which scripting technology you use)
- Background threads.

**Cons:**

- Configuration is currently a bit limited.
