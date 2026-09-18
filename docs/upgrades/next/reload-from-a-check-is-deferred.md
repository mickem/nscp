---
icon: "⏱️"
modules: [core, LUAScript, PythonScript, DotnetPlugins]
action: conditional
---
**A reload requested from inside a check is applied after the check returns.**
Nothing to do on a default install. A script that calls `core.reload("service")`
(or reloads the module it runs in) from a check now hands the reload to the
service's scheduler instead of running it on the thread serving the check:
`reload()` returns as soon as the reload is queued, and the new configuration
applies moments later. Reloading the whole service that way previously
restarted the listener that was serving the check from one of that listener's
own threads, which left it dead - `check_nrpe` timed out from then on. A reload
requested from anywhere else, including `reload("settings")` from a script,
still applies before the call returns. A script that reads its own settings
straight after the call should reload them a moment later, or request
`delayed,service` and poll.
