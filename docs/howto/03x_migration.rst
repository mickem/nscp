.. _how_to_migrating_to_041:

====================
 Migrating to 0.4.1
====================

Since 0.4.1 has now been released it is important to consider how this will affect existing users. In general there are three major strategies:

#. I am happy and don't want to change anything
#. I am willing to migrate manually
#. I Want to migrate automatically

Depending on your choice of route you will end up either with a brand new NSClient++ which is very powerful or one whilst being brand new will work very much like the old one. Thus the drawback to not migrating to the "new format" that you wont (easily) be able to use new features.
Your migration strategy is also affected by the fact that in the next 6-to-12 months NSClient++ 0.4.2 will be released which is also a technology shift and which might very well introduce some migration work as well.

Background (Whats has changed)
==============================

In 0.4.0 a new format for the configuration was introduced. In addition to this some modules were renamed, some were changed from an architectural perspective and some were deprecated. The reason for this is that the "old" NSCient++ was approaching 10 years and was simply not designed for the flexibility which it offers today.
I will readily admit that NSClient++ is the most flexible and powerful monitoring agent available in the open source space (and this is not PR speak it is the truth).
To accommodate this flexibility the format **had to change**, it would have been impossible to keep backwards compatibility.
But the changes are mainly structural and designed to make things look more similar so from a human perspective the change is minor.
For instance compare the NRPE server configuration:

**0.3.9:**

.. code-block:: ini

   [modules]
   NRPEListsner
   
   [NRPE]
   port = 1234
   allow_arguments = 1

**0.4.1:**

.. code-block:: ini

   [/modules]
   NRPEServer = enabled
   
   [/settings/NRPE/server]
   port = 1234
   allow arguments = 1

If we analyse this we can see four structural changes:

#. Servers are now called Servers (they used to be called Listeners)
   Thus NRPEListener is now NRPEServer

#. Syntax is now KEY=VALUE, before it was sometimes just KEY, and sometimes KEY=VALUE
   Thus where before it was module-name it is now module-name= enabled

#. Paths are now hierarchical and there are two reasons for this. The first one is that you can now load a module multiple times which requires you to configure it more than once which is impossible in a flat structure. The second reason is that where before it was 10-15 section there is now probably closer to hundreds.
   `NRPE <NRPE>`_ is now `/settings/NRPE/server </settings/NRPE/server>`_

#. Keys are now using space not dash, underscore nor space etc. This is the only change I really regret doing but the idea was to make it simpler since before there was no rule.
   Thus allowed-hosts is now "allowed hosts"

This is pretty much it in terms of structural changes. Some modules were changed, for instance NSCAAgent was split up into NSCAClient and Scheduler to allow for other passive protocols.

Future (What's to come)
=======================

So what will happen in the future?
Well, two tings. 

#. Next version will support Lua configuration (i.e. scripts) which will handle all migration so hopefully this is the last change.
#. Next version will feature a new command check subsystem

The first change is just good, the latter change might have impacts on how you use NSClient++ (it might be good to look into this if you're  planning to migrate). Just a quick recap (see milestone `milestone:0.4.2 <milestone:0.4.2>`_ for more details): checks will no longer support variable lists,  instead more common argument parsing system is used throughout. I think this is a simplification which helps since it is pretty confusing the way arguments are parsed today. For instance:

.. code-block:: ini

   1 = CheckCounter MaxWarn=10 Counter=\a\a MaxWarn=20
   2 = CheckCounter MaxWarn=10 Counter=\b\b MaxWarn=20 Counter=\c\c

Compare the following two aliases, what would you expect for bounds?

#. \a\a = 10 or 20? (Answer is: 20)
#. \b\b = 10 or 20? (Answer is: 10)
#. \c\c = 10 or 20? (Answer is: 20)

Pretty confusing right? In the future options will override each other so the answer in 0.4.2 will become:

#. \a\a = 10 or 20? (Answer is: 20)
#. \b\b = 10 or 20? (Answer is: 20)
#. \c\c = 10 or 20? (Answer is: 20)

Essentially rendering the Multiple MaxWarn statements meaningless.

But this also means that you can only have one bound for each check (something you can easily get around by using CheckMulti so I don't think it's a big deal). But if you are using a lot of Multiple MaxWarn/Crit statements you might take this time to either wait before you upgrade or start looking at ways to change your checks (like check multi).

.. code-block:: ini

   old = CheckCounter MaxWarn=10 Counter=\b\b MaxWarn=20 Counter=\c\c
   new = CheckMulti command="CheckCounter Counter=\b\b MaxWarn=10" command="MaxWarn=20 Counter=\c\c"

.. note::
   I also don't "officially" support 0.3.x any more. 
   This does not mean I wont help you but it does mean I reserve the right to say "sorry, you have to upgrade for that". or "can you reproduce on 0.4.1?". 
   I always try to help people but without a support team I only have so much time to dedicate to support.

Hands-on
========

I don't want to upgrade
-----------------------

This is fine and you can keep using the old format even with the new version without any problems.
It is important to understand the limitations by doing this:


* No new things: None of the new keys will be available to you unless you "include" a new syntax file.
* No automatic process: You cannot use the "nscp settings" command line for managing your configuration.
* No remote configuration (in 0.4.2): since the remote configuration tools will work using nscp settings they will not work either.
* No registry support: Old settings file ONLY work with the old ini file not the old registry concept. 

It is simple to keep the old format. When you upgrade you select "Old configuration" in the installer UI. And afterwards you can at any time run the following command to change the configuration file:

.. code-block:: bat

  nscp settings --switch old

To change to the old file. This assumes you have the file since it is not shipped with NSClient++ any more. If you want you can always get the old ini file from github here: `https://github.com/mickem/nscp/blob/0.3.9/NSC.dist <https://github.com/mickem/nscp/blob/0.3.9/NSC.dist>`_

I want to migrate
-----------------

Migrating automatically should work for most people and can be done from the installer or manually at a later time. It is very possible to install with "Old configuration" and then migrate at a later time.
To migrate the configuration you run the following command:

.. code-block:: bat

  nscp settings --migrate-to ini

I want to change by hand
------------------------

migrating by hand is perhaps also a valid option if you don't have too much configuration. In this case there is (in builds post 0.4.1.89) a sample (full) config file from which you can copy/paste the settings.
Another option is to use the settings command line tool to generate, add and remove default values.

See the guide on using settings command line interface (TODO).
In the mean time the following commands might be a pointer:

.. code-block:: bat

   nscp settings --help
   nscp settings --validate
   nscp settings --generate ini --add-defaults
   nscp settings --generate ini --remove-defaults
   nscp settings --path /settings/NRPE/server --key "allowed hosts" --set 127.0.0.1


Troubleshooting
---------------

**TODO**: Add this section!
