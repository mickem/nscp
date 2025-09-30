# Installing NSClient++

This is a grooving process before it was all manual but slowly we are getting a more "automated" installation process so
hopefully this will keep improving in the future as well and some of the steps might go away.


## Table of Contents

- [Introduction](#introduction)
  - [Configuration](#configuration)
  - [Windows Firewall](#windows-firewall)
- [Automated installation](#automated-installation)
  - [Basic command line](#basic-command-line)
  - [MSI Options](#msi-options)
  - [Features](#features)
  - [Silent install](#silent-install)
  - [Debugging](#debugging)
- [Specifying your monitoring tool](#specifying-your-monitoring-tool)
- [Copy configuration from a HTTP server](#copy-configuration-from-a-http-server)
- [Use configuration from a HTTP server](#use-configuration-from-a-http-server)

## Introduction

NSClient++ comes with an interactive installer (MSI) which is the preferred method of installation.
There is also a command line option for registering (and de-registering) the service but as the Installer is the
preferred way.

More detailed information on how to do silent installs and automated installs in large environments can be found below.
For most people installing NSClient++ is simply running the MSI entering some options and clicking next.
**BUT** this is only where the fun begins. After installing NSClient++ you need to configure it.

### Configuration

NSClient++ support multiple configuration storage backends but here we will assume you are using the ini file which is
the default.

To check where the configuration is stored, you can run the following command:

```
$ nscp settings --show
INI settings: (ini://${shared-path}/nsclient.ini, C:\source\build\x64\dev/nsclient.ini)
```

Now this configuration can include other configuration backends and files so your setup might be more complicated.
For details on the configuration options check the [the reference documentation](reference)

### Windows Firewall

1. Windows firewall exception for NRPE and check_nt is installed (optionally) by the installer.
   If you have another firewall then the built-in one you might have to manually add exceptions to all incoming traffic
   if you which to use check_nrpe and/or check_nt.
2. External Firewall (optional)

Firewall configuration should be pretty straight forward:

- If you use NRPEServer (check_nrpe, NRPEListener) you need the NRPE port open (usually 5666) from the Nagios server
  towards the client.
- If you use the NSClientServer (check_nt, NSClientListener) you need the (modified) NSClient port open (usually 12489)
  from the Nagios server towards the client.
- If you use the NSCA Module (passive checks) you need the NSCA port open from the client towards the Nagios server.
  client:* -> Nagios:5667
- Also be aware that ports are configurable so if you override the defaults you obviously need to update the firewall
  rules accordingly.
- There a multitude of other protocol which you can also use with NSClient++ (including, NRPE, NSCA, Syslog, SMTP, etc
  etc) so please review what your firewall setup in conjunction with you NSClient++ design.

| Protocol   | Source | Source port | Destination   | Destination port | Comment                                                        |
|------------|--------|-------------|---------------|------------------|----------------------------------------------------------------|
| NRPE       | Nagios | <all>       | client        | 5666             | The nagios server initiates a call to the client on port 5666  |
| NSClient   | Nagios | <all>       | client        | 12489            | The nagios server initiates a call to the client on port 12489 |
| NSCA       | client | <all>       | Nagios        | 5667             | The client initiates a call to the Nagios server on port 5667  |
| NRPE-proxy | client | <all>       | remote-client | 5666             | The client initiates a call to the remote client on port 5666  |

- **Nagios** is the IP/host of the main monitoring server
- client is the Windows computer where you have installed NSClient++
- remote-client is the "other" client you want to check from NSClient++ (using NSClient++ as a proxy)

All these ports can be changed so be sure to check your nsclient.ini for your ports.

## Automated installation

The NSClient++ installer for windows is a standard MSI installer which means it can be installed using pretty much all
deployment techniques available on the windows platform.

### Basic command line

To customize the installation you can use the standard MSI options to add/remove features and set properties.
An example of this is shown below.

```commandline
$ msiexec /i NSCP-VERSION-x64.msi ADDLOCAL=ALL REMOVE=PythonScript INSTALLLOCATION=c:\FooBar
```

### MSI Options

A list of all the MSI options can be found below.

| Keyword            | Description                                                                                                             |
|--------------------|-------------------------------------------------------------------------------------------------------------------------|
| INSTALLLOCATION    | Folder where NSClient++ is installed.                                                                                   |
| ADD_DEFAULTS       | Add default values to the configuration file.                                                                           |
| ALLOWED_HOSTS      | Set allowed hosts value                                                                                                 |
| CONFIGURATION_TYPE | Configuration context to use                                                                                            |
| CONF_CHECKS        | Enable default check plugins                                                                                            |
| CONF_NRPE          | Enable NRPE server                                                                                                      |
| CONF_NSCA          | Enable NSCA Collection /OU probably need scheduler as well)                                                             |
| CONF_NSCLIENT      | Enable NSClient Server (check_nt)                                                                                       |
| CONF_SCHEDULER     | Enable Scheduler (required by NSCA)                                                                                     |
| CONF_WEB           | Enabled WEB Server                                                                                                      |
| NRPEMODE           | NRPE Mode (LEGACY, SECURE for using ceretificates)                                                                      |
| NSCLIENT_PWD       | Password to use for check_nt (and web server)                                                                           |
| CONF_INCLUDES      | Additional files to include in the config syntax: <alias>;<file> For instance CONF_INCLUDES=op5;op5.ini;local;local.ini |
| OP5_SERVER         | OP5 Server if you want to automatically submit passive checks via Op5 northbound API.                                   |
| OP5_USER           | The username to login with on the OP5_SERVER                                                                            |
| OP5_PASSWORD       | The password to login with on the OP5_SERVER                                                                            |
| OP5_HOSTGROUPS     | Additional hostgroups to add to the host.                                                                               |
| OP5_CONTACTGROUP   | Additional contactgroups to add to the host.                                                                            |
| NO_SERVICE         | Set to 1 to disable installing the service (then you can manually create and activate the service when needed)          |
| TLS_VERSION        | The TLS version to use (1.0, 1.1, 1.2, *1.3*)                                                                           |
| TLS_VERIFY_MODE    | The TLS verify mode to use (*none*, peer, fail_if_no_peer_cert)                                                         |
| TLS_CA             | The CA file to use for TLS connections (if not using the system default)                                                |
| CONF_SET           | Set a configuration value in the form of section1;key1;value1;section2;key2;value2...                                   |

### Features

NSClient++ consists of the following features most which can be disabled when doing silent installations.

| Feature Name      | Title                  | Description                                                                               |
|-------------------|------------------------|-------------------------------------------------------------------------------------------|
| CheckPlugins      | Check Plugins          | Various plugins to check your system. (Includes all check plugins)                        |
| ExtraClientPlugin | Various client plugins | Plugins to connect to various systems such as syslog, graphite and smtp                   |
| FirewallConfig    | Firewall Exception     | A firewall exception to allow NSClient++ to open ports                                    |
| LuaScript         | Lua Scripting          | Allows running INTERNAL scripts written in Lua                                            |
| NRPEPlugins       | NRPE Support           | NRPE Server Plugin. Support for the more versatile NRPE protocol (check_nrpe)             |
| NSCAPlugin        | NSCA plugin            | Plugin to submit passive results to an NSCA server                                        |
| CheckMK           | Check MK support       | Experimental support for check_mk server and clients                                      |
| ElasticPlugin     | Elastic Search support | Support for submitting metrics to elastic                                                 |
| NSCPlugins        | check_nt support       | NSClient Server Plugin. Support for the old NSClient protocol (check_nt)                  |
| PythonScript      | Python Scripting       | Allows running INTERNAL scripts written in Python                                         |
| SampleScripts     | Scripts                | Scripts for checking and testing various aspects of your computer and NSClient++          |
| Shortcuts         | Shortcuts              | Main Service shortcuts                                                                    |
| WEBPlugins        | WEB Server             | NSClient WEB Server. Use this to administrate or check NSClient via a browser or REST API |
| OP5Montoring      | OP5 Monitoring         | Scripts/config for the op5 monitoring system.                                             |

### Silent install

Now we can put all this together using the normal silent installer which is again part of the standard windows install
toolkit.
So if you already have a framework for managing installs use that instead of this command line.
The gist of it is: `msiexec /quiet /i <MSI FILE> PROPERTY=PropertyValue ...`

For instance Installing NSClient++ into c:\foobar using registry as configuration and not installing the
Python script binaries.

```
msiexec /qn /l* log.txt /i NSCP-0.4.3.50-x64.msi INSTALLLOCATION=c:\FooBar CONFIGURATION_TYPE=registry://HKEY_LOCAL_MACHINE/software/NSClient++ ADDLOCAL=ALL REMOVE=PythonScript
```

### Debugging

If you run into any issues or want to report a bug a good first step is to enable logging of the installation process.
This is done by adding `/l* log.txt` to the command line (you can of course change the log file name to whatever you want).

> *BEWARE:* The log file can contain sensitive information such as passwords so be careful when sharing it with others.

## Specifying your monitoring tool

THere is an option which can be used to define a base-lin for your monitoring tool.
If you do not have a supported monitoring tool you can set it to "none" which will use the generic configuration which 
works with all monitoring tools.

```
msiexec /i NSCP-<version>.msi MONITORING_TOOL=none
```

### Silent op5 install

To enable active checks via NRPE from OP5 you can set the `MONITORING_TOOL` option to `OP5`.

```
msiexec /i NSClient++.msi MONITORING_TOOL=OP5
```

### Silent op5 install (Northbound)

To enable passive reports via OP5s Northbound API you can set the `OP5_SERVER`, `OP5_USER` and `OP5_PASSWORD` options.
In this case setting `MONITORING_TOOL` is done automatically when ever `OP5_SERVER` is detected.

```
msiexec /i NSClient++.msi OP5_SERVER=https://op5.com OP5_USER=monitor OP5_PASSWORD=rotinom
```

## Copy configuration from a HTTP server

Staring with version 0.10.5 it is possible to download a configuration file from a HTTP server and use that as
the configuration for NSClient++.

```
msiexec /i NSClient++.msi IMPORT_CONFIG=http://myserver.com/nsclient.ini
```
This will then copy the config file from the server and use that as the configuration file.
This is useful if you do not have a good way to manage Windows Machines.

## Use configuration from a HTTP server

A simple way to manage NSClient++ configuration is to use the HTTP configuration backend.
This way you can manage the configuration from a central server and all your clients will automatically pick up
the configuration from there.
Any changes you make to the configuration file on the server will automatically be picked up by the clients.

```
msiexec /i NSClient++.msi CONFIGURATION_TYPE=http://myserver.com/nsclient.ini
```
