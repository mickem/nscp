---
icon: "⏱️"
modules: [core, LUAScript, PythonScript, DotnetPlugins]
action: conditional
---
**A reload requested from inside a check is applied after the check returns.**
Nothing to do on a default install. A script that calls `core.reload(...)`
for the whole service or for any module from inside a check now hands the
reload to the service's scheduler instead of running it on the thread serving
the check: `reload()` returns as soon as the reload is queued, and the new
configuration applies moments later. That thread can belong to a module other
than the one running the script - a Lua check served over NRPE runs on an NRPE
thread - so reloading the service, or `NRPEServer` itself, that way previously
restarted the listener that was serving the check from one of that listener's
own threads, which left it dead: `check_nrpe` timed out from then on. A reload
requested from anywhere else, including `reload("settings")` from a script,
still applies before the call returns. A script that reads its own settings
straight after the call should reload them a moment later, or request
`delayed,service` and poll.

Where there is no scheduler to hand it to - `nscp unit`, and `nscp client`
without `--boot` - a script that reloads the whole service or the module it
is running in is refused with an error, as that would unload the script it
returns into. Reloading some other module from a script, which is how the
unit-test scripts apply the configuration they set, still runs immediately
there.
