---
title: "Concurrency audit: process-handle reuse, an exception escape and reload-time races"
fixed_in: next
severity: "Low"
modules: [core, CheckExternalScripts, CheckHelpers, NRPEServer, NSCAServer, NSClientServer, CheckMKServer, ElasticClient]
action: none
---
A threading and ordering review of the whole tree - data races, lock order,
lifetime, shutdown and reload sequencing. Most of what it found is a
correctness or availability problem rather than a security one, and those are
described in the [upgrade notes](../setup/upgrading.md). Three items are worth
recording here.

- **The Windows script launcher could terminate an unrelated process.** The
  list of running children that `kill_all()` walks at module unload stored bare
  `HANDLE` values, and the function that removes an entry gave up after a
  second under contention - leaving the value in the list while the spawn that
  owned it went on to close the handle. Windows reuses handle values, so
  `TerminateProcess` could then be aimed at whatever the service had opened
  since. Registration and removal now block instead of timing out, each entry
  carries its process id, and `kill_all()` re-checks the id before terminating.
  The same launcher leaked a process handle on every timed-out script and on
  every `capture output = false` spawn; both paths now close what they opened.
- **One exception could stop the agent.** The listener modules' io-pool threads
  ran `io_context::run()` with nothing above it, and `boost::thread` calls
  `std::terminate` on an uncaught exception. Over TLS the protocol was entered
  from the handshake completion handler, outside the catch-all that covered the
  plain-TCP path, so anything thrown while a connection was being set up - an
  NSCA encryption failure, a check_mk handler running Lua, an allocation
  failure anywhere in a handler - took the whole service down rather than that
  one connection. Both the pool thread body and the TLS start path are now
  guarded.
- **Credentials could be read while they were being rewritten.** A settings
  reload re-enters a module's `loadModuleEx` while its threads are running, and
  several modules assigned connection settings field by field with no
  synchronisation. In `ElasticClient` the fields include the Elasticsearch
  user, password and API key, read by the event, metrics and log paths on three
  different threads: a submission could pick up half-old credentials against a
  half-new address, or read a `std::string` mid-assignment. Those modules now
  build the new configuration aside and publish it in one step, so a submission
  uses one whole generation of settings from start to finish.

**What to do:** nothing required.
