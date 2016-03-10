NSClient++
==========
Stable 0.4.4: [![Build Status](https://travis-ci.org/mickem/nscp.png?branch=0.4.4)](https://travis-ci.org/mickem/nscp)

Master: [![Build Status](https://travis-ci.org/mickem/nscp.png?branch=master)](https://travis-ci.org/mickem/nscp)


NSClient++ (nscp) aims to be a simple yet powerful and secure monitoring daemon. 
It was built for Nagios/Icinga, but nothing in the daemon is Nagios/Icinga specific and it can be used in many other scenarios where you want to receive/distribute check metrics.

The daemon has 3 main functions:

* Allow a remote machine (monitoring server) to request commands to be run on this machine (the monitored machine) which return the status of the machine.
* Submit the same results to a remote (monitoring server).
* Take action and perform tasks.

NSClient++ can be found at: http://nsclient.org

Documentation can be found at: http://docs.nsclient.org

Extending NSClient++
--------------------

NSClient++ is designed to be open ended and allow you to customize it in any way you design thus extensibility is a core feature.

 * ExternalScripts responds to queries and are executed by the operating system and the results are returned as-is.
   This is generally the simplest way to extend NSClient++ as you can utilize whatever infrastructure or skill set you already have.
 * LuaScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
   This is the best option if you want to allow the script to run on any platform with as little infrastructure as possible.
 * PythonScripts are internal scripts which runs inside NSClient++ and performs various tasks and/or responds to queries.
   Python is an easy and powerful language but it requires you to also install python which is often not possible on server hardware.
 * .Net modules similar to Native modules below but written on the dot-net platform.
   This allows you to write components on top of the large dot-net ecosystem.
 * Modules are native plugins which can extend NSClient++ in pretty much any way possible.
   This is probably the most complicated way but gives you the most power and control.

Talking to NSClient++
---------------------

Since NSClient++ is meaningless by itself it also supports a lot of protocols to allow it to be used by a lot of monitoring solutions.

 * NRPE (Nagios Remote plugin Executor) is a Nagios centric protocol to collect remote metrics.
 * NSCA (Nagios Service Check Acceptor) is a Nagios centric protocol for submitting results.
 * NSCP is the native NSClient++ protocol (still under development)
 * dNSCP is a high performance distributed version of NSCP for high volume traffic.
 * NRDP is a replacement for NSCA.
 * check_mk is a protocol utilized by the check_mk monitoring system.
 * Syslog is a protocol primarily designed for submitting log records.
 * Graphite allows you do real-time graphing.
 * SMTP is more of a toy currently.

Supported OS/Platform
---------------------

NSClient++ should run on the following operating systems:
 * Windows: From NT4 SP5 up to Windows 2012R2 and Windows 8.1
 * Linux: Debian, Centos and Ubuntu (and possibly others as well)
 * Win32, x64 as well as various Linux hardware as well.
