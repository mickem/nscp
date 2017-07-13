# NSClient++ Theory

Some people say NSClient++ can feel a bit daunting the first time you encounter it.
This is often due to the number of options you have.
NSClient++ was designed to be open-ended meaning not restrict you in favor of allowing you to do what you need in the way you need it.
So before you start tinkering with NSClient++ it is important to first decide what you want to achieve.
The key decisions you have to make are related to (in no particular order):

-   Settings
    Where do you want to store your settings?
-   Protocols
    Which protocols do you need
-   Commands
    Which are right for you?

This section will go through the various categories one by one and give you some hints and ideas on how to proceed.
It is important to understand that these are examples and thus there are many reasons not to do what I suggest here but if you are new to NSClient++ you probably want to start out in the normal way.

## Settings

The first thing (at least in order of how you use NSClient++) to understand when it comes to settings is that at its core it is simply a key value store.
This means you have values (which you configure) and for each value you have a key consisting of a path and a key name.

One great feature of NSClient++ is that settings are "self describing" which means the application knows all available values what they are what they do.
So the best way to figure what an option is to ask NSClient++ which can be done in (among other ways):

*   By adding in default values and descriptions
*   By using the WEB-UI to configure NSClient++

The last thing you need to be aware of is that you can mix-and-mash this means you are not locked down to using one or even a single kind of settings store instead you can combine them to your hearts content.

While freedom can make things complicate the general rule is to try to keep from making things complicated.
Thus I recommend people to use a single ini file located in the root folder of the application directory.
This makes things simple to understand and simple to edit.

!!! danger
    Now this is not the way to go if you have a large and/or a complicated setups and neither if you want to automat your infrastructure but for starting out it is the way to go.

Now as the settings are self describing a good way to start is to add in all the description in your configuration file.
You can use the `nscp settings --generate` command which generates the settings file for you and make sure all the comments describing all your options are present.
If you want to add in all keys you are missing you can add the option `--add-defaults` to this command. So running `nscp settings --generate --add-defaults` will add all available keys for all currently active modules.

!!! note
    To remove the default values (and make your configuration much shorter) you can use the `nscp settings --generate --remove-defaults` command.

Apart from ini there are more ways to configure NSClient++ the full list of supported configuration stores are:

*   registry (windows registry)
    Great way to manage configuration centrally if you have a tool for managing your windows configuration.
*   http/https (load ini files over the network)
    Poor mans version of a central configuration repository you can control the configuration centrally but it is not as elegant as the registry.
*   old settings files
    Should never be used
*   dummy (no settings file instead you have a fully scripted setup)
    Might seem pointless but if you opt to go with scripts over configuration this might come in handy.
    It is also a great way to start NSClient++ without configuration `nscp test --settings dummy`

The last thing to be aware of is that you can include any number of another configuration stores from any given configuration store.
This means you can create a hierarchy of configuration files which is very useful if you want to allow local administrators to override the a centrally pushed configuration.

### Choosing settings

Thus to summaries when starting out you a simple ini file in the folder of the nsclp.exe file.
But be aware that you will need most likely move this to the registry and use includes to manage your environment in a larger setup and a great way to allow your local administrators to override configuration is to include a "local.ini" in which they can add any local overrides.

## Protocols

While NSClient++ supports a myriad of protocols most protocols are for specific needs and you will most likely start out using NRPE via `check_nrpe` as that is the simplest way to get started.

!!! danger
    Please be aware that NRPE is defaulted to "secure" mode which means that the insecure `check_nrpe` which usually ships with Nagios wont work unless you explicitly configures NSClient++ to support it.

A quick comparison of the protocols:

| Protocol | Mode              | Secure                   | Drawbacks                                            |
|----------|-------------------|--------------------------|------------------------------------------------------|
| NRPE     | Active (polling)  | Can be secured           | Default payloads lengths are 1024                    |
| NSCA     | Passive (pushing) | Secure                   | Default payloads lengths are 512                     |
| NRDP     | Passive (pushing) | Can be secured           |                                                      |
| REST     | Multiple          | Hopefully secure         | NSClient++ only.                                     |
| check_nt | Active (polling)  | No encryption            | No encryption/authentication as well as very limited |
| check_mk | Active (polling)  | Not secure               | No encryption, enforces client configuration, slow.  |
| Graphite | Graphing          | Not secure               | Only for graphing                                    |
| SMTP     | Passive (pushing) | Not secure in NSClient++ | Mainly a toy                                         |
| Syslog   | Passive (logs)    | Not secure               | Mainly for sending logs                              |

The common protocols (for checks) are NRPE, NSCA and Graphite.

From an NSClient++ and configuration point of view most of these protocols will appear very similar.
This means that for instance switching from NSCA to NRDP will be a pretty small change.
So once you have setup one it is easy to setup the other and easy to switch.

### Choosing protocols

When it comes to picking a protocol I would say go with the infrastructure you have.
So if you are using NSCA passively on your current machines: Stick with that for NSClient++.
If you don't already have any infrastructure in place I would recommend:

*   NRPE for Active monitoring (i.e. Central monitoring server polls for status)
*   NSCA for Passive monitoring (i.e. NSClient++ pushes status to Central monitoring server)
*   Graphite for graphing solutions

The reason I don't recommend REST is that it is new in 0.5.0 and currently lacks some smoothness of NRPE (and some features of NSCA).

## Commands

Command are much up to what you want to monitor.
Thus if you want to monitor cpu usage you will want to use `check_cpu`.
Be aware that a lot of monitoring is pointless and will produce many false positives specially in large environments.
Thus it is a good idea to decide if you are interested in brad overall health (infrastructure monitoring) or specifics (application monitoring).

### infrastructure monitoring

If you are monitoring many servers which run different types of applications.

*   `check_drivesize`
*   `check_memory`
*   `check_cpu`
*   `check_network`
*   `check_nscp`
*   `check_eventlog`

### application monitoring

If you want to monitor the health of an application running on a server.

*   `check_process`
*   `check_service`
*   `check_eventlog`
*   `check_tasksched`
*   `check_files`
*   `check_logfile`
