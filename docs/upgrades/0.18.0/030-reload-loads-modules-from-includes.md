---
icon: "⚙️"
modules: [core]
action: none
---
**A reload now loads modules enabled in an included file since the last one.**
An `[/includes]` file is read once when the configuration is loaded and served
from memory after that, so a module switched on in one — most importantly
`fleet.ini`, which the fleet sync rewrites whenever a bundle changes — was not
actually loaded until the service next restarted. Nothing said so: the file on
disk was right and a fleet host reported itself in sync, while the module
quietly did nothing. The visible case was a fleet bundle turning on
`NRDPClient` and `Scheduler` to start passive submissions, which then never
arrived. A reload now re-reads the included files first, so settings changed
in them take effect and newly enabled modules are loaded; modules already
running are left alone. Two things are unchanged: a module *disabled* since
the last load keeps running until the service restarts, and `[/modules]` in
the main `nsclient.ini` is still only read at startup.
