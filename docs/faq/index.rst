.. _faq-index:

#################################
 FAQ: Frequently asked questions
#################################

1. Problems
========

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

1.4 "EventlogBuffer? is too small
**********************************
This is because you have one or more entries in your eventlog which are larger then the "default buffer size of 64k". The best way to fix this is to increase the buffer used.

.. code-block:: ini

  [/settings/eventlog]
  buffer_size=128000

1.5 How do I properly escape spaces in strings
***********************************************

When you need to put spaces in a string you do the following:
 * nagios:
   - As usual you can do it anyway you like but I prefer: check_nrpe ... 'this is a string'

1.6 How do I properly escape $ in strings
******************************************

From:
 * nagios:
   - $$ (you use two $ signs)
 * from NSClient++
   - $ (you do not need to escape them at all)

1.7 System Tray does not work
******************************
 **NOTICE**
 System tray is currently disabled and will be added back at some point

1.8 I get <insert random error from nagios here>
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

1.9 I use version 0.3.9 or 0.2.7
********************************

please upgrade to 0.4.1 and see if the error still persist before you ask questions and/or report bugs

1.10 Rejected connection from: <ip address here>
************************************************
This is due to invalid configuration.
One important thing you '''NEED''' to configure is which hosts are allowed to connect. If this configuration is missing or invalid you will get the following error:

.. code-block:: log

  013-04-02 16:34:07: e:D:\source\nscp\trunk\include\check_nt/server/protocol.hpp:65: Rejected connection from: ::ffff:10.83.14.251

To resolve this please update your configuration:

.. code-block:: ini

  [/settings/default]
  
  ; ALLOWED HOSTS - A coma separated list of hosts which are allowed to connect. You can use netmasks (/ syntax) or * to create ranges.
  allowed hosts = <ADD YOUR NAGIOS 1 IP HERE>,<ADD YOUR NAGIOS 2 IP HERE>,10.11.12.0/24

1.11 Arguments via NRPE
***********************

For details see :ref:`how_to_external_scripts`

1.12 Nasty metacharacters
*************************

If you get illegal metachars or similar errors you are sending characters which are considered harmful through NRPE.
This is a security measure inherited from the regular NRPE client.

The following characters are considered harmful: |`&><'\"\\[]{}
To enable this in the NRPE server you can add the following (please '''notice''' the same issue is also valid for CheckExternalScripts if you are using scripts see question 12 for details):
.. code-block:: ini

  [/settings/NRPE/server]
  allow nasty characters=true
