---
icon: "⏱️ 🔧"
modules: [core, WEBServer, NRPEServer, NSCAServer, CheckMKServer]
action: conditional
---
**Reloads now wait for the checks that are running, and a module can no longer
unload or restart itself from inside a request it is serving.** Nothing to do
on a default install; the changes are visible only in a few specific setups.

* A settings reload holds new checks off a module while its `loadModuleEx`
  applies the new configuration, and waits up to five seconds for the checks
  already inside it to return. A reload is therefore no longer instantaneous on
  a busy agent, and a check arriving during one waits rather than running
  against half-applied settings. If a check holds a module for longer than five
  seconds the reload proceeds anyway and logs which module it was.
* Unloading the module that is serving the request is refused and answers with
  an error. In practice this is `POST /api/v2/modules/WEBServer/commands/unload`
  (or the same through `put_module` or the console), which previously took the
  agent down with it. Unload `WEBServer` from the command line or restart the
  service instead.
* Reloading a listener module from inside a check that the same listener is
  serving - a script calling `core.reload("NRPEServer")` from a check invoked
  over NRPE - is refused with an error instead of leaving that listener dead.
  Reload it from another transport, or reload the service as a whole. A reload
  that does not touch the module serving the request is unaffected and still
  applies before the call returns.
* Configuration downloaded over HTTP (`[/settings] 1 = http://...`) now gives
  up on a read or write that stalls for 30 seconds instead of waiting
  indefinitely. A settings server that used to take longer than that to answer
  will now fail the refresh and keep the cached copy.
* Submissions over NRPE, NSCA, NSCP and check_mk apply the configured timeout
  to connecting and to the TLS handshake, not just to the exchange. A target
  that accepts a connection and then goes quiet now fails its timeout instead
  of holding a scheduler thread.
