---
icon: "🔧"
modules: [CheckExternalScripts]
action: conditional
---
**`kill tree` now works on Linux, and unloading the module ends running
scripts.** The setting was accepted on Linux but did nothing: on a timeout
only the script itself was signalled, so a helper it had backgrounded kept
running and, because it still held the script's output pipe, the check only
came back when the timeout expired. Unloading the module did not touch
running scripts at all. Now:

- With `kill tree = true` the script runs in a session of its own (so also a
  process group of its own, with no controlling terminal), and the timeout's
  `SIGTERM` and `SIGKILL` go to the whole group: everything the script started
  dies with it.
- Unloading the module kills every running script. With `kill tree = true`
  that includes its helpers. Without it only the script itself is killed: a
  helper it backgrounded keeps running, and the check waits for its timeout.
- A script ended by an unload reports `was killed: the module is unloading`,
  and one ended by a signal reports which (`was terminated by signal 9
  (SIGKILL)`), instead of an empty `UNKNOWN`.
- If reading a script's output fails, the script is now ended the same way
  as on a timeout and reports `failed while its output was being read;
  killed`. Before, the agent waited for it with no deadline.
- Every script reads stdin from `/dev/null`. Under the service that is what
  it already had; under `nscp test` a script no longer reads the terminal.

Check your scripts only if you set `kill tree = true` on Linux. A helper that
must outlive its check has to leave the group (start it with the `setsid`
utility). A script that calls `setsid()` itself now gets `EPERM`, because it
already leads its session; the `setsid` utility forks first and is unaffected.
