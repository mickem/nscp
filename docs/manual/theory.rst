#################################
 Getting-Started with NSClient++
#################################

Some people say NSClient++ can feel a bit daunting the first time you encounter it.
This is often due to the number of options you have. NSClient++ was designed to be open-ended meaning not restrict you in favor of allowing you to do what you need in the way you need it.
Thus if you start with a clean slate you need to understand what options you have and make the correct choices.
So before you start tinkering with NSClient++ it is important to first decide what you want to achieve.
With 0.4.0 and soon to be release 1.4.1 there is a multitude of protocols and way you can use NSClient ++. 
The key decisions you have to make are related to (in no particular order):


* Settings
   Where do you want to store your settings?

* Protocols
   Which protocols do you need

* Commands
   Which are right for you?

So lets go through some of the the options one by one and try to explain how you can make simple decisions. It is important to understand that the recommendations I make here are general thus there are many reasons not to follow them but if you are new to NSClient++ you probably want to start this way and change later on.

Settings
========

The first thing (at least in order of how you use NSClient++) to understand when it comes to settings is that at its core it is simply a key valus store.
This means you have values (which you configure) and for each value you have a path and a key much like you have for any other application.

Another important thing to understand is that it is "self describing" this means the application knows all available values what they are what they do.
So the best way to figure what an option is is to ask NSClient++.

And finally you need to be aware that you can mix-and-mash this means you are not locked down to using one or even a single kind you can combine them to your hearts content.

All this freedom can make things complicate but in general most of it is not something you need so the best way to simplify this is simply to ignore it (if you don't want it to be complicate don't make it complicated).
Thus I recommend people to use a single ini file located in the root folder of the application directory.
This makes things simple to understand and simple to edit. Now this is obviously not the way to go if you have complicated setups but we are looking for simplicity here. I the future the settings file will be moved to a "better folder" (read local system profile folder) to better follow the Windows/Linux guide lines.

So the simple example is: a single "ini-file" called nsclient.ini which contains all you options and all your settings.

Now I said it is self describing this means the application can tell you which options you have this is done using the
{{{nscp settings --generate}}} command which generates the settings file for you now to make this simple this will only add in the descriptions you lack.
If you want to add in all keys you are missing you need to add the option --add-defaults as well so to summaries: Running {{{nscp settings --generate --add-defaults}}} will update your config file with all available options (for the modules you have activated).


Protocols
=========

Another thing which can seem daunting at first is sheer number of supported protocols.
And again this is to allow you to use utilize your current infrastructure (not force you to change to something fancy NSClient++ specific).
So the simple solution here is simply: Continue using what you use today!

If you are not using a protocol today and are planning to implement 0.4.1 you might want to consider NSCP as it is the most powerful protocol available but it is also the most unstable (since it is in development).

A quick comparison of the protocols:

============ ================= ===================== ===================================
**Protocol** **Mode**          **Secure**            **Drawbacks**
============ ================= ===================== ===================================
NRPE         Active (polling)  Somewhat secure       Default payloads lengths are 1024
NSCA         Passive (pushing) Somewhat secure       Default payloads lengths are 512
NRDP         Passive (pushing) Somewhat secure
NSCP         Multiple          Hopefully secure      NSCLient++ only, and in development. '''Requires 0.4.1'''
check_nt     Active (polling)  Not considered secure No encryption/authentication as well as very limited
check_mk     Active (polling)  Not secure            No encryption, enforces client configuration, slow. '''Requires 0.4.1'''
Graphite     Graphing          Not secure            Only for graphing
SMTP         Passive (pushing) Not currently secure  Mainly a toy
Syslog       Passive (logs)    Not secure            Mainly for sending logs
dNSCP        Multiple          Maybe secure          Distributed massive scalable protocol for high performance scenarios. '''Requires 0.4.2'''
============ ================= ===================== ===================================

The common protocols (for checks) are NRPE and NSCA.

But the most important thing to understand is that they all work the same (they are configured the same, and they look the same and they behave the same).
So once you have setup one it is easy to setup the other and easy to switch.

Commands
========

The most complicated thing about commands are the various "types" of commands which can be used.


#. Internal commands (supplied out the box)
#. External scripts (some supplied out of the box)
#. Python Scripts (internal)
#. Lua scripts (internal)
#. .net "plugins" (don't think there actually are any)

In general the internal commands are probably the most used after that comes scripts which usually downloads from nagios exchange or some such.
SO this is not very confusing either really..

So now that we have the areas we need to decides:
Where we shall store settings, and unless you really need something else use nsclient.ini
Which protocol to use, if you don't have a clue and are using Nagios you are probably looking for NRPE.
Finally commands the easy way is to look at the various check commands on the wiki and then move on to nagios exchange.
