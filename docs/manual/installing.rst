.. _manual_installing:

#######################
 Installing NSClient++
#######################

This is a grooving process before it was all manual but slowly we are getting a more "automated" installation process so hopefully this will keep improving in the future as well and some of the steps might go away.

1. Installation
===============

NSClient++ comes with an interactive installer (MSI) which should preferably be used. There is also a command line option for registering (and de-registering) the service -- for details refer to the `manual installation guide <wiki/doc/installation/manual>`_. If you are using Windows NT4 there is some dependencies you need to install manually -- for details refer to the `NT4 Dependency guide <wiki/doc/installation/nt4>`_.

Details on how to do silent installs and automated silent installs can be found at the `Installation Guide <wiki/guides/install>`_ page.

Thus to install the Client you simply click the MSI package (for your platform) and follow the wizard through. **BUT** after you have installed it still needs to be configured as follows.

2. Configuration
================

Before you start NSClient++ you need to configure it by editing the configuration. The configuration is usually in a file called NSC.ini or nsclient.ini (depending on version). But where the configuration is actually stored can differ so the best way to make sure is to open the file called  boot.ini which containes the order the file are loaded. It is also important top understand that configuration can include other configuration stores which means one file can for instance include the registry or some such.
For details on the configuration options check the `Configuration <wiki/doc/configuration>`_ section of the wiki.

It is important to understand that out-of-the-box NSClient++ has no active configuration which means you **NEED** to be configured it before you can use it. This is for obviously because of security so no one will accidentally install this and get potential security issues on their servers.

3. System tray
==============

The system tray has been temporarily removed in the 0.4.0 version it will be added bgack in a future version much improved. Since the system tray is not really an essential feature this is deemed low priority but the idea is to replace it with a configuration interface and proper client.

For 0.3.x you can still us either the old SystemTray module or the newer shared_session as before. For details on this see the `System tray installation guide <wiki/doc/installation/systray>`_.

4. Testing and Debugging
========================

After you have installed NSClient++ you need to start it. As it is a normal Windows service you can either use the "net start" and "net stop" commands in the command line or you can use the Computer Manager's Services node.

When you are starting and/or configuring your client you can use the "debug" mode which will be very helpful as you will see the debug log in "real time" when you play around with it. To start NSClient++ in test/debug mode use the following command:

.. code-block:: bat

nscp test

5. Windows Firewall
===================

A windows firewall exception for NRPE and check_nt is installed (optional) if you have another firewall then the built-in one you might have to manually add exceptions to all incoming traafic if you wich to use check_nrpe and/or check_nt.

6. External Firewall (optional)
===============================

Firewall configuration should be pretty straight forward:

- If you use NRPEServer (check_nrpe, NRPEListener) you need the NRPE port open (usually 5666) from the nagios server towards the client.
- If you use the NSClientServer (check_nt, NSClientListener) you need the (modified) NSClient port open (usually 12489) from the nagios server towards the client.
- If you use the NSCA Module (passive checks) you need the NSCA port open from the client towards the nagios server.
 client:* -> nagios:5667

- Also be aware that ports are configurable so if you override the defaults you obviously need to update the firewall rules accordingly.
- There a multitude of other protocol which you can also use with NSCLient++ (including, NRPE, NSCA, Syslog, SMTP, etc etc) so please review what your firewall setup in conjunction with you NSClient++ design.

========== ========== =========== ============= ================ ======================
Protocol   Source     Source port Destination   Destination port Comment
========== ========== =========== ============= ================ ======================
NRPE       **nagios** <all>       client        5666             The nagios server initiates a call to the client on port 5666
NSClient   **nagios** <all>       client        12489            The nagios server initiates a call to the client on port 12489
NSCA       client     <all>       **nagios**    5667             The client initiates a call to the nagios server on port 5667
NRPE-proxy client     <all>       remote-client 5666             The client initiates a call to the remote client on port 5666
========== ========== =========== ============= ================ ======================

- **nagios** is the IP/host of the main nagios server
- client is the Windows computer where you have installed NSClient++
- remote-client is the "other" client you want to check from NSClient++ (using NSClient++ as a proxy)

All these ports can be changed -- check your nsclient.ini.
