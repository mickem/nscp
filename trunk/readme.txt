** ROUGH DRAFT **
** Spelling missate etc... :) **


NSClient++ is a windows service that allows performance metrics to be gathered by Nagios 
(and possibly other monitoring tools). It is an attempt to create a NSClient compatible 
but yet extendable performance service for windows.


Installation:
NSClient++ -install
Will install the NT service

NSClient++ -uninstall
Will uninstall the service

NSClient++ -start
Will start the service

NSClient++ -stop
Will stop the service

NSClient++ -test
Will start in interactive mode:
enter exit<return> to exit. ANything else will be interpreted as a command ie: foo&bar<enter> will be interpreted as a command name foo with argument bar.
It also listens to the port in this mode so it can be used to run the client standalone.
Notice output is *NOT* piped t othe stdard output unless you enable the console logger module.





The API is quite simple:
The following functions are available for a module to "export": (think DLL)

NSLoadModule
- Loading of modules (done when the service starts)
NSGetModuleName
- Used to get a nice name for the module.
NSGetModuleVersion
- Not really used but...
NSHasCommandHandler
- Let the "core" know it can handle commands (from nagios)
NSHasMessageHandler
- Used to let the "core" know it wants to see all log/debug/error messages that are generated.
NSHandleMessage
- Used to digest a log/debug/error message.
NSHandleCommand
- Used to (possibly) digest a command.
When a command is "injected" then command is parsed and then sent along to all plugins (which accept commands) in order (they are listed in NSC.ini) and they are allowed to act on the command. If they do act on the command the "injection" is aborted and the resulting "return string" is returned to the "injecter" (normaly check_nt through a TCP stream. The ordering can thus be used to 1, optimize if things are slow (though I fail to see that unless someone makes some pretty f*cked up modules) and priority if one module overlaps the command of another (though then I would recommend someone to change the plugin command strings :)

NSUnloadModule
- Used to unload and terminate "stuff" nicely. (Generally this is when the service terminates)
NSModuleHelperInit
- Used to send along callbacks to the module (for making calls to the "core". (Ie. To allow the module to inject commands, get versions, names, and settings)

So the API is quite simple and straight forward (I hope :)

To make a simple call graph:

* Service starts:
* For each plugin
        - call NSLoadModule
        - call NSModuleHelperInit
        - if NSHasCommandHandler
                . Send to "command plugin" stack
        - if NSHasMessageHandler
                . Send to "message plugin" stack
* Wait for socket data
        - Parse socket data
        - For each "command plugin"
                . call NSHandleCommand
                . if Command handled abort

<termination signal sent to service>
* For each plugin
        - NSUnloadModule
* Terminate service

<Log/debug/error message generated in a module>
* On incoming message:
        - For each "message plugin"
                . call NSHandleMessage

There are a few more things to this but overall I have tried to keep things simple.

The extensions (to the API) I'm considering as of now (depending on the fallout) is:
* "SettingsHandler" to allow registry/INI/* settings modules. As the overhead of reg/ini settings system is quite low, perhaps this is overkill. But I don't know: thinking wildly there might be a reason for someone to want to load settings from a central server or whatever :)

* "SocketHandlers" though this will not be a specific handler I have considered to refactor out the code and allow the socket handler (that reads the TCP socket and parses the command) to be a plugin when I do SSL as I assume SSL will come with quite a high overhead and thus I might not need it all the time.

Also as mentioned this might be a good place to do a NRPE socket parser module to allow compatibility with NRPE. But this I assume is a bit in the future as I need to get "NSClient++" before I can get "NRPEClient++"...

As for PassiveChecks that could be implemented as a plugin today as any plugin can inject commands into the "core". Thus a plugin can be loaded, have a background thread that every X minutes calls "core".injectCommand(); and sends the result over to nagios. (This is how I assume that passive checks work)




// Michael Medin - michael <at> medin <dot> name - http://www.medin.name
