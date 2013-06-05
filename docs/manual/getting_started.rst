.. _manual-getting_started:

############################
 Getting started with 0.4.x
############################

This is a hands on guide to getting started with NSClient++.
We will start with a clean slate and work our way in small easy to follow steps towards a fully functional monitoring solution.

**THIS is a work in progress!**

For the sake of simplicity this will be based on a Windows version.
If you would like to do this on Linux it "should" work much the same apart from some of the system specific checks will not be avalible for your plattform.

Getting it!
===========

The first thing we want to do is fetching and unzipping it!
We will not use the installer here since we want to really explain everything and the installer can sometimes hide some things for you.
<<IMAGE>>
After you unzip it you should have the following folder structure if you don't please don't continue instead try to figure out why you do not :)

Now that we (presumably) have NSClient++ unzipped let start by starting it.
Since much of what NSClient++ does requires "elevated privileged" you should always run NSClient++ in a "Administrator command prompt".
<<IMAGE>>

Now change directory to where you unzipped NSClient++ and run nscp without any arguments like so:

.. code-block:: bat

  cd to/your/nsclient++/folder
  nscp
  ...


Running nscp without arguments will show you the initial help screen.

Getting Help
============
The next step is to explore the various "modes".

.. code-block:: bat

  nscp --help
  Allowed options:
  
  Common options:
    --settings arg        Override (temporarily) settings subsystem to use
    --help                produce help message
    --debug               Set log level to debug (and show debug information)
    --log arg             The log level to use
    --version             Show version information
  ...
  Unit-test Options:
    -l [ --language ] arg Language tests are written in
    -a [ --argument ] arg List of arguments (gets -- prefixed automatically)
    --raw-argument arg    List of arguments (does not get -- prefixed)

First argument has to be one of the following: client, help, service, settings, test, unit,
Or on of the following client aliases: eventlog, lua, nrpe, nsca, nscp, py, python, sys, syslog, wmi,


As you can see there is a nice little help screen describing most of the options you can use.
An important thing to understand is that like many other command line tools such as git or subversion the first "option" is not an option instead it acts as a "mode of operation".
In our case we have the following modes:

||client||Run client side commands||
||help||Display help screen||
||service||Control the service as well as (un)install it||
||settings||Work with the settings subsystem||
||test||run nsclient++ in the famouse "test" mode||
||unit||Execute unit tests||

In addition to this we have alias for various "common" client modules. They are mearly a glorified way to save yourself some typing for instance {{{nscp sys}}} will yield the same result as saying {{{nscp client --module CheckSystem}}}.


* eventlog
* lua
* nrpe
* nsca
* nscp
* py
* python
* sys
* syslog
* wmi


Getting setup!
==============

Since NSClient++ is an agent or daemon or service or whatever you want to call it. It is generally designed to run by it self without user intervention.
Thus it requires a lot of configuration to know what to do so next up on our guide is getting to know the configuration and the interface.

So the first thing we need to decide is where to place our configuration since I like the simplicity of the ini file I tend to opt for an inifile in the "current folder".
This, though, a rather crappy place to have the configuration and breaks both the Linux and Windows design guidelines but I tend to opt for simplicity. 
The long term plan (read 0.4.4) plan is to some day move the settings file over to where settings files should be stored.
So the first thing you need to do now is tell NSClient++ this but before we get ahead of our self lets look at how we can manipulate the settings via the nscp command line interface. If you remember from the getting help section above we had a "settings" mode of operation. Lets look into that one a bit more.


.. code-block:: text

  nscp settings --help
  Allowed options (settings):
  
  Common options:
    --settings arg        Override (temporarily) settings subsystem to use
    --help                produce help message
    --debug               Set log level to debug (and show debug information)
    --log arg             The log level to use
    --version             Show version information
  
  Settings options:
    --migrate-to arg      Migrate (copy) settings from current store to target
                          store
    --migrate-from arg    Migrate (copy) settings from current store to target
                          store
    --generate arg        (re)Generate a commented settings store or similar KEY
                          can be trac, settings or the target store.
    --add-defaults        Add all default (if missing) values.
    --validate            Validate the current configuration (or a given
                          configuration).
    --load-all            Load all plugins (currently only used with generate).
    --path arg            Path of key to work with.
    --key arg             Key to work with.
    --set arg             Set a key and path to a given value.
    --switch arg          Set default context to use (similar to migrate but does
                          NOT copy values)
    --show                Set a value given a key and path.
    --list                Set all keys below the path (or root).


In our case what we want is something which goes by the fancy name of "set default context". 
This has the option --switch and takes a single argument which defines the settings system to "switch to".
Notice the comment about difference between the various --migrate-xxx options and switch. Switch will not migrate your current settings.
Using migrate here would thus copy all settings from whatever settings you are using today to the new one befor updateing the settings to use.

.. code-block:: text

  d:\source\nscp\build\x64>nscp settings --switch ini://${exe-path}/nscp.ini
  Current settings instance loaded:
    INI settings: (ini://${exe-path}/nscp.ini, d:/source/nscp/build/x64//nscp.ini)

What this does is configure NSClient++ to use the nsclient.ini config file and that the fie is placed in the ${exe-path} folder (which is the same path as the exe file you are launching it from is placed).
But how does it do this you ask? What does actually change when you run this command?
And the answer is simply a file called boot.ini is updated. This file describes where all settings files are found (and any configuration the settings file might require). Go ahead try it, delete this file and re-run the above command and it will come back looking the same.

So now that we actually have a configuration file what can we do with it?
If you read the theoretical version of the getting-started page you know by now that NSClient++ settings are self-describing.
The command to for this is:

.. code-block:: python

  nscp settings --generate ini --add-defaults --load-all

The "--add-missing" will force NSClient++ to add all missing keys to the settings store. The previous name for this option was --add-defaults which is the same.

So lets go ahead and run this command and see what our nsclient.ini file looks like.
If you open up the file you will be pleasantly (or not) surprised it has very few options.
The reason for this is the modular nature of NSClient++ with a clean install there are no modules configured so we only get configuration options for the "core program" which really has very little in the way of configuration.

Getting modular
===============

Loading modules is the most important aspect of NSClient++ and there is plenty to choose from.
NSClient++ 0.4.1 has over 30 different modules.
Modules can be grouped into three generic kinds of modules.

#. CheckModules
    They provide various checkmetrics and commands for checking your system.

#. Protocol providers (Servers and clients)
    They provide the communication protocols you can use when connection NSClient++ to the outside world.

#. Scripting modules
    They provide additional features in the form of scripts and even other modules. I tend to think of them as proxies.

We will start exploring "check-modules" here as they are the simplest form of module.
Now comes a hefty dose of Linux hate. This guide will use the CheckSystem module which is (currently) only available on Windows.
So how do we load modules?
The simple way is to use the NSClient++ command line syntax here as well.

.. code-block:: bat

  nscp settings --activate-module CheckSystem --add-missing

You should by now be able to guess what this command will do.
First it will attempt to load the module if that succeed it will enable the module and add all new keys which the module provides.
In this case the checksystem module is not very configurable but there were a few new things.
As always open up the config file and see what was added.

Getting your hands dirty
========================

So now that we have a module loaded lets move on to actually using the module.
The best (and most ignored) way to work with NSClient++ is to use the "test mode".
Test mode provides you with two things.

#. A real-time debug log of what NSClient++ does
#. A way to run commands quickly and easily and see the debug log at the same time.

To start test mode you run the following command:
nscp test

This will print some debug log messages and eventually leave you with blinking cursor.

.. code-block:: text

  d:\source\nscp\build\x64>nscp test
  d vice\logger_impl.cpp:373  Creating logger: console
  d rvice\NSClient++.cpp:382  NSClient++ 0,4,1,37 2012-08-11 x64 Loading settings and logger...
  d ngs_manager_impl.cpp:162  Boot.ini found in: d:/source/nscp/build/x64//boot.ini
  d ngs_manager_impl.cpp:178  Boot order: ini://${exe-path}/nsclient.ini
  d ngs_manager_impl.cpp:181  Activating: ini://${exe-path}/nsclient.ini
  d ngs_manager_impl.cpp:73   Creating instance for: ini://${exe-path}/nsclient.ini
  d mpl/settings_ini.hpp:275  Reading INI settings from: d:/source/nscp/build/x64//nsclient.ini
  d mpl/settings_ini.hpp:241  Loading: d:/source/nscp/build/x64//nsclient.ini from ini://${exe-path}/nsclient.ini
  l rvice\NSClient++.cpp:393  NSClient++ 0,4,1,37 2012-08-11 x64 booting...
  d rvice\NSClient++.cpp:394  Booted settings subsystem...
  e rvice\NSClient++.cpp:483  Warning Not compiled with google breakpad support!
  d rvice\NSClient++.cpp:540  booting::loading plugins
  d rvice\NSClient++.cpp:306  Found: CheckSystem
  d rvice\NSClient++.cpp:840  addPlugin(d:/source/nscp/build/x64//modules/CheckSystem.dll as )
  d rvice\NSClient++.cpp:817  Loading plugin: CheckSystem
  d stem\CheckSystem.cpp:103  Found alternate key for uptime: \2\674
  d stem\CheckSystem.cpp:114  Found alternate key for memory commit limit: \4\30
  d stem\CheckSystem.cpp:125  Found alternate key for memory commit bytes: \4\26
  d stem\CheckSystem.cpp:136  Found alternate key for cpu: \238(_total)\6
  d rvice\NSClient++.cpp:612  NSClient++ - 0,4,1,37 2012-08-11 Started!
  d tem\PDHCollector.cpp:94   Loading counter: cpu = \238(_total)\6
  l ce\simple_client.hpp:29   Service seems to be started (Sockets and such will probably not work)...
  d tem\PDHCollector.cpp:94   Loading counter: memory commit bytes = \4\26
  l ce\simple_client.hpp:32   Enter command to inject or exit to terminate...
  d tem\PDHCollector.cpp:94   Loading counter: memory commit limit = \4\30
  d tem\PDHCollector.cpp:94   Loading counter: uptime = \2\674


Now you can enter commands.
For instance if you start by entering the commands command

.. code-block:: text

  commands
  l ce\simple_client.hpp:54   Commands:
  l ce\simple_client.hpp:57   | check_cpu: Check that the load of the CPU(s) are within bounds.
  l ce\simple_client.hpp:57   | check_memory: Check free/used memory on the system.
  l ce\simple_client.hpp:57   | check_pdh: Check a PDH counter.
  l ce\simple_client.hpp:57   | check_process: Check the state of one or more of the processes running on the comput
  er.
  l ce\simple_client.hpp:57   | check_registry: Check values in the registry.
  l ce\simple_client.hpp:57   | check_service: Check the state of one or more of the computer services.
  l ce\simple_client.hpp:57   | check_uptime: Check time since last server re-boot.
  l ce\simple_client.hpp:57   | checkcounter: Check a PDH counter.
  l ce\simple_client.hpp:57   | checkcpu: Check that the load of the CPU(s) are within bounds.
  l ce\simple_client.hpp:57   | checkmem: Check free/used memory on the system.
  l ce\simple_client.hpp:57   | checkprocstate: Check the state of one or more of the processes running on the compu
  ter.
  l ce\simple_client.hpp:57   | checkservicestate: Check the state of one or more of the computer services.
  l ce\simple_client.hpp:57   | checksingleregentry: Check values in the registry.
  l ce\simple_client.hpp:57   | checkuptime: Check time since last server re-boot.
  l ce\simple_client.hpp:57   | listcounterinstances: *DEPRECATED* List all instances for a counter.

You get a list of all commands you can execute. commands in this context is actual check commands which generaly checks some aspect of you system.
Lets try out the first one:

.. code-block:: text

  check_cpu
  d rvice\NSClient++.cpp:933  Injecting: check_cpu...
  d rvice\NSClient++.cpp:958  Result check_cpu: WARNING
  l ce\simple_client.hpp:80   WARNING:ERROR: Usage: check_cpu <threshold> <time1> [<time2>...] (check_cpu MaxWarn=80 time=5m)

As you can see this returns a warning and tells us some general information how to use it.
Now this is more of an exception then a rule but it it the idea hence fort to try to make commands and such "helpful".

Now lets move on to trying to run the actual suggested commands:

.. code-block:: text

  check_cpu MaxWarn=80 time=5m
  d rvice\NSClient++.cpp:933  Injecting: check_cpu...
  d rvice\NSClient++.cpp:958  Result check_cpu: OK
  l ce\simple_client.hpp:80   OK:OK CPU Load ok.
  l ce\simple_client.hpp:82    Performance data: '5m'=22%;80;0

Now it seems to actually do something.

So there we have it the system is now being monitored (albeit manually by you but we will resolve that in the next section).

Getting Connected
=================
Now that we have a sense of how to check our data we shall start connecting our self with the outside world so our monitoring agent can connect and see if we are actually working properly.

**TODO**

Getting scheduled
=================

**TODO**

Getting to the end
==================

**TODO**

SO now we have walked through the basics of setting up NSClient++ some of this requires Windows some requires 0.4.1 and some requires manual work.
Most of this  can be automated and/or configured from the installer but I think it is better to understand what actually happens and I hope this gives a sense of how NSClient++ works and how you can use NSClient++.


