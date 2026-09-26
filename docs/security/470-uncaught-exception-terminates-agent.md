---
title: "An uncaught exception on a worker thread could terminate the agent"
fixed_in: next
severity: "Low"
modules: [core, NRPEServer, NSClientServer, CheckMKServer, WEBServer, CheckDisk, CheckSystem, GearmanClient]
action: none
---
Hardening, not a reported vulnerability. No input or condition is known to
reach this, and the paths that matter most were already guarded: the listener
protocols wrap the part that runs a check in `catch (...)`, the socket
connection state machine wraps its dispatch, and the collectors catch around
each fetch.

What was missing was the guarantee. NSClient++ wraps every module entry point
in a `try`/`catch`, but a background worker thread is not an entry point:
nothing sits between its body and the C++ runtime, so an exception that escaped
one called `std::terminate()` and ended the whole agent rather than one piece
of work. Whether one could escape depended on each thread body having
remembered to catch, and the coverage was uneven - the CheckSystem auxiliary
collector and the Gearman job runner had no top-level catch at all, several
sites caught `std::exception` but not everything, and code sitting between the
inner `try` blocks (buffer growth, settings parsing, string building) was
covered by neither. The socket server pool ran `io_context::run()` bare, so
anything that did escape a completion handler would have ended a pool worker;
on a bare thread that is the process.

Every background thread now starts through a guard
(`threads::start_guarded_thread`): what escapes is logged as
`Thread '<name>': terminated by an uncaught exception: <details>` and the
thread ends instead of the process. Modules log that line at `critical` and
the core at `error`, so alerting on the text rather than on the severity is
what covers every path. Event loops re-enter `run()`, so a request that throws
fails as a request and the queued work continues. Underneath all of it, a
terminate handler now writes a report naming the exception and the thread to
`nsclient.fatal` before the process goes down, so the paths nobody has thought
of yet stop being silent. On Windows that handler is installed per worker
thread, because MSVC keeps `std::set_terminate` state per thread and one
installed on the main thread would have left every worker - the network-facing
socket server pool included - terminating in silence.

**What to do:** nothing. If an agent has been disappearing without explanation,
check for a `nsclient.fatal` next to `nsclient.log` after upgrading - it will
now say what happened. If that folder cannot be written the report goes to a
private per-account folder under the system temp directory
(`/tmp/nsclient++-<uid>/`, `%TEMP%\nsclient++\`) created with owner-only
permissions, never to a fixed name directly in a world-writable directory;
the startup log says which file is in use.
