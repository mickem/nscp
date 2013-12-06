.. _faq_index:

#################################
 FAQ: Frequently asked questions
#################################

1. Problems
===========

1.1 I am having problems where do I start?
******************************************

  NSCP has a built-in "test and debug" mode that you can activate with the following command
  
..  code-block:: bat

    nscp test

What this does is two things. 
1. it starts the daemon as "usual" with the same configuration and such.
2. it enables debug logging and logs to the console.
This makes it quite easy to see what is going on and why things go wrong.

1.2 Failed to open performance counters
***************************************
 * The first thing to check is the version. If you are using an old version (pre 0.3.x) upgrade!
 * Second thing to check is whether the servers' performance counters working?
   Sometimes the performance counters end up broken and need to be rebuilt there is a command to validate performance counters:

..  code-block:: bat

    nscp sys --validate

For details: Microsoft KB: http://support.microsoft.com/kb/300956 essentially you need to use the "lodctr /R" command.

1.3 Bind failed
****************
 Usually this is due to running more then once instance of NSClient++ or possibly running another program that uses the same port.
  - Make sure you don't have any other instance NSCLient++ started.
  - Check if the port is in use (netstat -a look for LISTENING)

1.4 EventlogBuffer is too small
**********************************
This is because you have one or more entries in your eventlog which are larger then the "default buffer size of 64k". The best way to fix this is to increase the buffer used.

.. code-block:: ini

  [/settings/eventlog]
  buffer_size=128000

1.5 System Tray does not work
******************************
 **NOTICE**
 System tray is currently disabled and will be added back at some point

1.6 I get <insert random error from nagios here>
*************************************************

This information is usually useless to me since the error in nagios is not related to the problem.
This is due to most protocols supported by nagios does not support reporting errors only status.
To see the error do the following:

.. code-block:: bat

  net stop nscp
  nscp test --log info
  ... wait for errors to be reported ...
  exit
  net start nscp

To get the debug log do the following:

.. code-block:: bat

  net stop nscp
  nscp test --log debug
  ... wait for errors to be reported ...
  exit
  net start nscp

Please check and include this information before you submit questions and/or bug reports.

1.7 High CPU load and check_eventlog
*************************************

Som people experience high CPU load when checking the event log this can usualy be resolved using the new command line option scan-range setting it to the time region you want to check

.. code-block:: bat

   CheckEventLog ... scan-range=12h ...

1.8 Return code of 139 is out of bounds
***************************************

This means something is wrong. To find out what is wrong you need to check the NSClient++ log file.
The message means that an plugin returned an invalid exit code and there can be many reasons for this but most likely something is miss configured in NSClient++ or a script your using is not working.
So the only way to diagnose this is to check the NSClient++ log.

One simple way to show the log is to run in test mode like so:

.. code-block:: bat

  net stop nscp
  nscp test
  ...
  # wait for error here
  ...
  exit
  net start nscp

.. note::
  But it is impossible to tell what is wrong without the NSClient++ log.

1.9 Enable debug log
********************

By default the log level is info which means to see debug messages you need to enable debug log::

	[/settings/log]
	file name = nsclient.log
	level = debug

2. Escaping and Strings
=======================

2.1 How do I properly escape spaces in strings
***********************************************

When you need to put spaces in a string you do the following:
 * nagios:
   - As usual you can do it anyway you like but I prefer: check_nrpe ... 'this is a string'

2.2 How do I properly escape $ in strings
******************************************

Dollar signs are "strange" in nagios nad has to be escaped using double $$s

From:
 * nagios:
   - $$ (you use two $ signs)
 * from NSClient++
   - $ (you do not need to escape them at all)

2.3 How do I properly escape \ in strings
*****************************************

Backslashes and som other control characters are handled by the shell in Nagios and thus escaped as such.

From:
 * nagios:
   - "...\\..."
 * from NSClient++
   - "...\\..."

2.4 Arguments via NRPE
**********************

For details see :ref:`how_to_external_scripts`

2.5 Nasty metacharacters
*************************

If you get illegal metachars or similar errors you are sending characters which are considered harmful through NRPE.
This is a security measure inherited from the regular NRPE client.

The following characters are considered harmful: |`&><'\"\\[]{}
To work around this you have two options.

1. You can enable it
2. You can switch most commands to not use nasty characters

To enable this in the NRPE server you can add the following:

.. code-block:: ini

   [/settings/NRPE/server]
   allow nasty characters=true
   
   [/settings/external scripts]
   allow nasty characters=true

To not use nasty characters you can replace man y of them in built-in commands:

========== ===========
Expression Replacement
========== ===========
>          gt
<          lt
'..'       s(...)
${..}      %(..)
========== ===========

3. General
==========
   
3.1 I use version 0.3.9 or 0.2.7
********************************

please upgrade to 0.4.1 and see if the error still persist before you ask questions and/or report bugs

3.2 Rejected connection from: <ip address here>
************************************************

This is due to invalid configuration.
One important thing you '''NEED''' to configure is which hosts are allowed to connect. If this configuration is missing or invalid you will get the following error:

.. code-block:: text

  013-04-02 16:34:07: e:D:\source\nscp\trunk\include\check_nt/server/protocol.hpp:65: Rejected connection from: ::ffff:10.83.14.251

To resolve this please update your configuration:

.. code-block:: ini

  [/settings/default]
  
  ; ALLOWED HOSTS - A coma separated list of hosts which are allowed to connect. You can use netmasks (/ syntax) or * to create ranges.
  allowed hosts = <ADD YOUR NAGIOS 1 IP HERE>,<ADD YOUR NAGIOS 2 IP HERE>,10.11.12.0/24

3.3 Timeout issues
*******************

Configuring timeouts can some times be a problem and cause strange errors.
It is important to understand that timeouts are cascading this means if you have all timeouts set to 60 seconds they will all miss fire.

.. image:: images/timeouts.png

The nagios server timeout will fire after exactly 60 seconds but the script timeouts will be started m,aybe 1 second after the nagios service check timeout this means once we reach 60 seconds the nagios service timeout will fire first and 1 second after the script will timeout. This you always have to set each timeout slightly less to accomodate this drift.

If your command takes 60 seconds you need to set the timeouts like this:

1. Script timeout: 60s

.. code-block:: ini
  
  [/settings/external scripts/wrappings]
  vbs = cscript.exe //T:120 //NoLogo scripts\\lib\\wrapper.vbs %SCRIPT% %ARGS%

2. External script timeout: 65 seconds

.. code-block:: ini
  
  [/settings/external scripts]
  timeout = 65

3. NRPE/server timeout: 70s

.. code-block:: ini
  
  [settings/NPRE/server]
  timeout = 70

4. check_nrpe timeout: 75s

.. code-block:: sh
  
  check_nrpe -t 75

5. nagios service check timeout: 80s

.. code-block:: ini

  service_check_timeout=80

3.4 Rotate log files

Rotating logfile can be done when size reaches a certain level (in this case 2048000 bytes)::

	[/settings/log]
	date setting = %Y.%m.%d %H:%M:%S
	file name = nsclient.log
	level = info
	[/settings/log/file]
	max size = 2048000
