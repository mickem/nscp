---
title: "An uncaught exception on a worker thread terminated the agent"
fixed_in: next
severity: "Low"
modules: [core, NRPEServer, NSClientServer, CheckMKServer, WEBServer, CheckDisk, CheckSystem, GearmanClient]
action: none
---
NSClient++ wraps every module entry point in a `try`/`catch`, but a background
worker thread is not an entry point: nothing sits between its body and the C++
runtime. An exception that escaped one called `std::terminate()` and the whole
agent went down - every check on the host, not just the one that failed.

The socket server pool made that reachable from the network. Completion
handlers run the listener's protocol, and the protocol runs a check, but the
pool threads called `io_context::run()` bare. Anything a handler threw
propagated out of `run()` and off the end of the thread, so a single request
that provoked an unhandled exception anywhere in the request path could stop
the agent for every consumer of it. The web server had the same shape with one
thread: the exception was logged, but the event loop was gone and nothing
restarted it, so the server stayed up and answered nothing.

Several collector threads - the CheckDisk collector, the CheckSystem 1 Hz and
auxiliary collectors, the Linux `CheckSystemUnix` collector, the Gearman job
runner - had no top-level guard either, so an unexpected failure in a provider
could end the process rather than one sample.

Every background thread now starts through a guard
(`threads::start_guarded_thread`): what escapes is logged as
`Thread '<name>': terminated by an uncaught exception: <details>` and the
thread ends instead of the process. Modules log that line at `critical` and
the core at `error`, so alerting on the text rather than on the severity is
what covers every path. Event loops go further and re-enter `run()`, so a
request that throws fails as a request and the server keeps serving.
Underneath all of it, a terminate handler now writes a report naming the
exception and the thread to `nsclient.fatal` before the process goes down, so
the paths nobody has thought of yet stop being silent. On Windows that handler
is installed per worker thread, because MSVC keeps `std::set_terminate` state
per thread and one installed on the main thread would have left every worker -
the network-facing socket server pool included - terminating in silence.

**What to do:** nothing. If an agent has been disappearing without explanation,
check for a `nsclient.fatal` next to `nsclient.log` after upgrading - it will
now say what happened. If that folder cannot be written the report goes to a
private per-account folder under the system temp directory
(`/tmp/nsclient++-<uid>/`, `%TEMP%\nsclient++\`) created with owner-only
permissions, never to a fixed name directly in a world-writable directory;
the startup log says which file is in use.
