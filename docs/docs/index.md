
# About NSClient++

NSClient++ (nscp) aims to be a simple yet powerful and secure monitoring daemon.
It was built for Nagios/Icinga/Neamon, but nothing in the daemon is Nagios/Icinga/Neamon specific and it can be used in many other scenarios where you want to receive/distribute check metrics.

The deamon has some basic features:

*   Allow a remote machine (monitoring server) to request commands to be run on this machine (the monitored machine) which return the status of the machine.
*   Submit the same results to a remote (monitoring server)
*   Take action and perform tasks
*   Submit metrics and real-time data to a central repository

## Extending NSClient++

NSClient++ is designed to be open ended and allow you to customize it in any way you design thus extensibility is a core feature.

-   ExternalScripts responds to queries and are executed by the operating system and the results are returned as-is.
    This is generally the simplest way to extend NSClient++ as you can utilize whatever infrastructure or skill set you   already have.
    The drawback is that external scripts cannot interact with the internals NSClient++ and thus they are limited in what   they can do.
-   LuaScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
    Lua is a popular embedded language which has a slightly arcane syntax but it is very efficient and capable and comes   bundled with NSClient++.
-   PythonScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
    Python is an easy to learn yet powerful language which comes bundled with NSClient++.
-   .Net modules similar to Native modules below but written on the dot-net platform.
    This allows you to write components on top of the large dot-net ecosystem. This makes it easy to develop check modules   for in house developed solutions for instance if you have dot-net competence in-house.
-   Modules are native plugins which can extend NSClient++ in pretty much any way possible.
    This is probably the most complicated way but gives you the most power and control.


## Talking to NSClient++

Since NSClient++ is not very useful alone it also supports a lot of protocols to allow it to communicate with various monitoring solutions.

-   NRPE (Nagios Remote plugin Executor) is a Nagios centric protocol to collect remote metrics.
-   NSCA (Nagios Service Check Acceptor) is a Nagios centric protocol for submitting results.
-   REST is the native NSClient++ protocol (still under development) which allows you to interact with NSClient++ over the http(s) protocol.
-   NRDP is a php replacement for NSCA developed by Nagios Inc(TM).
-   check_mk is a protocol utilized by the check_mk monitoring system. Check-mk support is in development.
-   Syslog is a protocol primarily designed for forwarding log records.
-   Graphite allows you do real-time graphing by sending metrics to graphite.
-   SMTP allows you to send email directly from NSClient++.

| NSClient++                  | check_nt   | check_nrpe 2.x | check_nscp_nrpe | check_nscp_web | check_nscp |
|-----------------------------|------------|----------------|-----------------|----------------|------------|
| 0.2.x                       | X          | X              |                 |                |            |
| 0.3.x                       | X          | X              |                 |                |            |
| 0.4.0                       | X          | X              |                 |                |            |
| 0.4.1                       | X          | X              |                 |                |            |
| 0.4.2                       | X          | X              |                 |                |            |
| 0.4.3                       | X          | X              | X               |                |            |
| 0.4.4                       | X          | X              | X               |                |            |
| 0.5.0                       | X          | X              | X               | X              |            |
| 0.5.1                       | X          | X              | X               | X              |            |
| 0.5.2                       | X          | X              | X               | X              | X          |
| 0.5.3 (forthcoming version) | X          | X              | X               | X              | X          |
| 0.6.0 (forthcoming version) | deprecated | X              | X               | Hopefully      | X          |

In 0.6.0 we will **NOT** remove `check_nt` but mark them as deprecated since there is no real reason to use it anymore.

## Supported OS/Platform

NSClient++ should run on the following operating systems:

-   Windows: From XP and up (including Windows 10 and Windows Server 2012)
-   Linux: Debian, Centos and Ubuntu (and possibly others as well)
-   Win32, x64 as well as various Linux hardware as well.
