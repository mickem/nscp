.. _manual_nscp_usage-index:

#######
 Usage
#######

Using NSClient++ is pretty straight forward. It is a normal command line program which works similarly to cvs, git and various other module based commands. This means that the first given argument is a "mode" or "module" or whatever you want to call it. Regardless of name it will dictate which options are available and which actions will be taken.

Contents:

.. toctree::
   :maxdepth: 2

   nscp_client.rst
   nscp_service.rst
   nscp_settings.rst
   nscp_test.rst

The modes which are currently in 0.4.x is:

* **client**
   Run as a client, useful for accessing and controlling remote hosts as well as trying out commands and/or using NSClient++ from a script.

* **test**
   The famous "test mode" where you can view the log interactively and debug various issues ranging from connectivity to command syntax.

* **settings**
   Manipulate the settings in various forms. Ranging from migrate between format and stores to settings values.

* **service**
   Control the service (on Windows) with options such as install/uninstall as well as start and stop.

* **help**
   Get help on available options and modes.

* '''<short hand tag>'''
   There is a set of short hands for using various built-in modules in client mode (this is the same as running nscp client --module <module name>.
   Available (subject to change) short hands are:

* nrpe (NRPEClient)
* nscp (NSCPClient)
* nsca (NSCAClient)
* eventlog (CheckEventLog)
* python (PythonScript)
* py (PythonScript)
* lua (LuaScript)
* syslog (SyslogClient)

client
======

The main goal of client is to do things you would normally do remotely, locally. For instance, you would normally run CheckProcess via check_nrpe from your Nagios machine but you can do the exact same using {{{nscp client --query CheckProcess}}}. This can be useful in many instances such as when debugging commands (locally) or executing check_nrpe from a windows machine etc.

Client has three (ish) modes of operation:

* --query
   To execute a query against a module.

* --exec
  To execute a command against a module.

* --submit
  To submit a response via any of the passive protocols.

For details on how to use client mode refer to `wiki /doc/usage/0.4.x/client <wiki//doc/usage/0.4.x/client>`_ for details.

test
====

This is the famous (?) --test mode of operation which is similar to client mode in many ways. The main difference is that test mode runs interactively (in contrast client mode has to have all commands specified on the command line). The main use for test mode is when you have problems with your monitoring and need to troubleshoot.

Start NSClient++ in test mode and see the errors as they happen, this is otherwise difficult to detect.

For details on how to use test mode refer to `wiki /doc/usage/0.4.x/test <wiki//doc/usage/0.4.x/test>`_ for details.

settings
========

Used for manipulating the settings store in any conceivable way. Everything from switching context to migrating settings to settings keys and generating documentation.

For details on how to use settings mode refer to `wiki /doc/usage/0.4.x/settings <wiki//doc/usage/0.4.x/settings>`_ for details.

service
=======

Used for manipulating the windows service.

For details on how to use service mode refer to `wiki /doc/usage/0.4.x/service <wiki//doc/usage/0.4.x/service>`_ for details.

help
====

Gives you help (duh).

For details on how to use settings mode refer to someone else for details :)