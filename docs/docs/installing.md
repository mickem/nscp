# Installing NSClient++

This is a grooving process before it was all manual but slowly we are getting a more "automated" installation process so hopefully this will keep improving in the future as well and some of the steps might go away.

## Installation (Simple)

NSClient++ comes with an interactive installer (MSI) which is the preferred method of installation.
There is also a command line option for registering (and de-registering) the service but as the Installer works pretty well it is the preferred way.

More detailed information on how to do silent installs and automated installs in large environments can be found below.
For most people installing NSClient++ is simply running the MSI entering some options and clicking next.
**BUT** this is only where the fun begins. After installing NSClient++ you need to configure it.

## Configuration

Before you start NSClient++ you need to configure it by editing the configuration. The configuration is usually in a file called nsclient.ini.
But the configuration can be stored elsewhere as will (for instance registry is a great place on Windows).

To check where the configuration is stored you can trun the following command:
```
$ nscp settings --show
INI settings: (ini://${shared-path}/nsclient.ini, C:\source\build\x64\dev/nsclient.ini)
```

Now this configuration can include other configuration files so you need to check that as well. So it is possible to include the registry from the ini file and vice versa.
For details on the configuration options check the [the reference documentation](reference)

## Windows Firewall

1.  Windows firewall exception for NRPE and check_nt is installed (optionally) by the installer.
    If you have another firewall then the built-in one you might have to manually add exceptions to all incoming traffic if you which to use check_nrpe and/or check_nt.
2.  External Firewall (optional)


Firewall configuration should be pretty straight forward:

-   If you use NRPEServer (check_nrpe, NRPEListener) you need the NRPE port open (usually 5666) from the Nagios server towards the client.
-   If you use the NSClientServer (check_nt, NSClientListener) you need the (modified) NSClient port open (usually 12489) from the Nagios server towards the client.
-   If you use the NSCA Module (passive checks) you need the NSCA port open from the client towards the Nagios server.
    client:* -> Nagios:5667
-   Also be aware that ports are configurable so if you override the defaults you obviously need to update the firewall rules accordingly.
-   There a multitude of other protocol which you can also use with NSClient++ (including, NRPE, NSCA, Syslog, SMTP, etc etc) so please review what your firewall setup in conjunction with you NSClient++ design.


| Protocol   | Source | Source port | Destination   | Destination port | Comment                                                        |
|------------|--------|-------------|---------------|------------------|----------------------------------------------------------------|
| NRPE       | Nagios | <all>       | client        | 5666             | The nagios server initiates a call to the client on port 5666  |
| NSClient   | Nagios | <all>       | client        | 12489            | The nagios server initiates a call to the client on port 12489 |
| NSCA       | client | <all>       | Nagios        | 5667             | The client initiates a call to the Nagios server on port 5667  |
| NRPE-proxy | client | <all>       | remote-client | 5666             | The client initiates a call to the remote client on port 5666  |

-   **Nagios** is the IP/host of the main monitoring server
-   client is the Windows computer where you have installed NSClient++
-   remote-client is the "other" client you want to check from NSClient++ (using NSClient++ as a proxy)

All these ports can be changed so be sure to check your nsclient.ini for your ports.

## Automated installation

The NSClient++ installer for windows is a standard MSI installer which means it can be installed using pretty much all deployment techniques available on the windows platform.
This means that there is no built-in deploy and configuration mechanism in NSClient++ instead it fully relies on standard tools provided for the Windows platform.
MSI files can be tweaked in several ways.

### Configuration options

There are several options for deploying configuration and copying text-file is probably the worst of them.
You can use group policies to push the configuration files but there are several other ways to do the same.

### MSI Options

The MSI file can be customized during the installer. The following keys are available:

| Keyword            | Description                                                                                                             |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------- |
| INSTALLLOCATION    | Folder where NSClient++ is installed.                                                                                   |
| CONF_CAN_CHANGE    | Has to be set for all configuration changes to be applied.                                                              |
| ADD_DEFAULTS       | Add default values to the configuration file.                                                                           |
| ALLOWED_HOSTS      | Set allowed hosts value                                                                                                 |
| CONFIGURATION_TYPE | Configuration context to use                                                                                            |
| CONF_CHECKS        | Enable default check plugins                                                                                            |
| CONF_NRPE          | Enable NRPE server                                                                                                      |
| CONF_NSCA          | Enable NSCA Collection /OU probably need scheduler as well)                                                             |
| CONF_NSCLIENT      | Enable NSClient Server (check_nt)                                                                                       |
| CONF_SCHEDULER     | Enable Scheduler (required by NSCA)                                                                                     |
| CONF_WEB           | Enabled WEB Server                                                                                                      |
| NRPEMODE           | NRPE Mode (LEGACY = default old insecure SSL, SAFE = new secure SSL)                                                    |
| NSCLIENT_PWD       | Password to use for check_nt (and web server)                                                                           |
| CONF_INCLUDES      | Additional files to include in the config syntax: <alias>;<file> For instance CONF_INCLUDES=op5;op5.ini;local;local.ini |
| OP5_SERVER         | OP5 Server if you want to automatically submit passive checks via Op5 northbound API.                                   |
| OP5_USER           | The username to login with on the OP5_SERVER                                                                            |
| OP5_PASSWORD       | The password to login with on the OP5_SERVER                                                                            |
| OP5_HOSTGROUPS     | Additional hostgroups to add to the host.                                                                               |
| OP5_CONTACTGROUP   | Additional contactgroups to add to the host.                                                                            |
|                    |                                                                                                                         |

### Features

NSClient++ consists of the following features most which can be disable when doing silent installs.

| Feature Name        | Title                  | Description                                                                               |
| ------------------- | ---------------------- | ----------------------------------------------------------------------------------------- |
| CheckPlugins        | Check Plugins          | Various plugins to check your system. (Includes all check plugins)                        |
| Documentation       | Documentation (pdf)    | Documentation for NSClient++ and how to use it from Nagios                                |
| DotNetPluginSupport | .net plugin support    | Support for loading modules written in .dot net (Requires installing .net framework)      |
| ExtraClientPlugin   | Various client plugins | Plugins to connect to various systems such as syslog, graphite and smtp                   |
| FirewallConfig      | Firewall Exception     | A firewall exception to allow NSClient++ to open ports                                    |
| LuaScript           | Lua Scripting          | Allows running INTERNAL scripts written in Lua                                            |
| NRPEPlugins         | NRPE Support           | NRPE Server Plugin. Support for the more versatile NRPE protocol (check_nrpe)             |
| NSCAPlugin          | NSCA plugin            | Plugin to submit passive results to an NSCA server                                        |
| NSCPlugins          | check_nt support       | NSClient Server Plugin. Support for the old NSClient protocol (check_nt)                  |
| PythonScript        | Python Scripting       | Allows running INTERNAL scripts written in Python                                         |
| SampleConfig        | Sample config          | Sample config file (with all options)                                                     |
| SampleScripts       | Scripts                | Scripts for checking and testing various aspects of your computer and NSClient++          |
| Shortcuts           | Shortcuts              | Main Service shortcuts                                                                    |
| WEBPlugins          | WEB Server             | NSClient WEB Server. Use this to administrate or check NSClient via a browser or REST API |
| OP5Montoring        | OP5 Monitoring         | Scripts/config for the op5 monitoring system.                                             |

### Silent install

Now we can put all this together using the normal silent installer which is again part of the standard windows install toolkit.
So if you already have a framework for managing installs use that instead of this command line.
The gist of it is: `msiexec /quiet /i <MSI FILE> PROPERTY=PropertyValue ...`

For instance Installing (with log) NSClient++ into c:\foobar using registry as configuration and not installing the Python script binaries.

```
msiexec /qn /l* log.txt /i NSCP-0.4.3.50-x64.msi INSTALLLOCATION=c:\FooBar CONFIGURATION_TYPE=registry://HKEY_LOCAL_MACHINE/software/NSClient++ ADDLOCAL=ALL REMOVE=PythonScript
```

#### Using Silent install in 0.4.4 and 0.5.0

You need to add two options in these version if you plan to update the configuration in a silent install:

```
msiexec /i NSCP-<version>.msi CONF_CAN_CHANGE=1 MONITORING_TOOL=none
```

CONF_CAN_CHANGE forces the config to become writable (if you run silently the detection never happens so this flag is never updated).
MONITORING_TOOL If no monitoring tool is specified it will default to "default" and overwrite various options given on the command line.

#### Silent op5 install

To enable active checks via NRPE from OP5 you can set the `MONITORING_TOOL` option to `OP5`.

```
msiexec /i NSClient++.msi MONITORING_TOOL=OP5
```

#### Silent op5 install (Northbound)

To enable passive reports via OP5s Northbound API you can set the `OP5_SERVER`, `OP5_USER` and `OP5_PASSWORD` options.
In this case setting `MONITORING_TOOL` is done automatically when ever `OP5_SERVER` is detected.

```
msiexec /i NSClient++.msi OP5_SERVER=https://op5.com OP5_USER=monitor OP5_PASSWORD=rotinom
```

## Multiple NSClient++

As NSClient++ uninstalls it self if you install there are two options for running multiple NSClient++ on a machine.

1.  You can add multiple services for the same installation
2.  You can install NSClient++_Secondary binary to get two instance on the same machine
3.  You can manually install NSClient++ (allows any number of installs)

To add multiple service you need to first create the services: `nscp service --install --name nscp2`

And then edit the start command so you can override the configuration.
The key to look for in the registry is `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\nscp2` and there you can modify the ImagePath:
`"C:\Program Files\NSClient++\nscp.exe" service --run --name nscp --settings ini://${shared-path}/nsclient-2.ini`
