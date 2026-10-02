---
icon: "🔧"
modules: [CheckExternalScripts]
action: conditional
---
**`kill tree` now works on Linux.** The setting was accepted on Linux but did
nothing: on a timeout only the script itself was signalled, so a helper it
had backgrounded kept running past the timeout and past module unload, and,
because that helper still held the script's output pipe, the check only came
back when the timeout expired. With `kill tree = true` the script is now
started in a process group of its own and the whole group gets the `SIGTERM`
(then `SIGKILL`) on timeout, so everything the script started dies with it;
unloading the module ends every running script the same way, which it never
did on Linux before. Nothing to do unless you set `kill tree = true` on a
Linux host and relied on a helper outliving its check: that helper now needs
to detach into its own session (`setsid`) to survive.
