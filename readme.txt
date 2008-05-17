NSClient++ is a windows service that allows performance metrics to be gathered by Nagios 
(and possibly other monitoring tools). It is an attempt to create a NSClient and NRPE compatible 
but yet extendable performance service for windows.

NSClient
The following commands (from check_nt) are supported.
 * CLIENTVERSION
 * CPULOAD
 * UPTIME
 * USEDDISKSPACE
 * MEMUSE
 * SERVICESTATE
 * PROCSTATE

NRPE
All scripts "should" work (haven't actually tried myself though).
Notice that encryption is still missing.

Installation:
To install simply copy all files to directory on the server and run 
the following command: "NSClient++ /install" to un-install run: 
"NSClient++ /uninstall".

The NSClient++ has the following command syntax:
NSClient++ -<command>
  <command>
    install	- Install the service
    start	- Start the service
    stop	- Stop the service
    about	- Show some info (version et.al.)
    version	- Show some info
    test	- Run interactively. Useful for debugging purposes.

The directory structure:
  <install root>
    - NSClient++.exe	- The executable (and service)
    - NSC.ini			- The INI file (settings)
    - changelog			- "Whats new"
    - *.dll				- Various runtime DLLs
    - readme.txt		- This file
    <modules>
      -	Various NSClient++ modules available to this instance.

A quick description of the included modules:
* CheckDisk.dll
	Dick check plug-in (not very complete as of yet).
* CheckEventLog.dll
	A very basic event log checker (HINT feedback wanted :)
* FileLogger.dll
	Log messages to file (Useful when reporting bugs, also please enable debug=1)
* NRPEListener.dll
	NRPE client code. Both listener and executer. 
	Might in the future rename this to NRPE or I will split the module into two.
* NSClientCompat.dll
	NSCLient compatibility module. Provides NSClient commands.
	This module will be migrated in to various other modules in the "near" future.
* NSClientListener.dll
	The NSClient listener.
* SysTray.dll
	Shows a system tray when the service (exe) is started and allows you to view the log file and inject commands.

Settings:
For details refer to NSC.ini.

About NSClient++
URL: http://www.sf.net/projects/nscplus

Contact the author:
Michael Medin
EMail: michael <at> medin <dot> name
Address: (feel free to send a postcard if you find this useful :)
Michael Medin
Terapivaegen 6b 3tr
SE-141 55 HUDDINGE
SWEDEN