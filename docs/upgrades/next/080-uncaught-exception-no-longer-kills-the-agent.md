---
icon: "🔒 💥"
modules: [core]
action: conditional
---
**A failing background task can no longer take the whole agent down.**
Check this if you alert on the NSClient++ process disappearing. This is not
the fix for a crash anyone has reported: most of the agent's background work
was already wrapped in its own `try`/`catch`, and there is no known input or
condition that took an agent down this way. What changes is the guarantee.
An exception that escaped a worker thread called `std::terminate()` and ended
the entire process, and whether one could escape came down to each thread body
remembering to catch - a convention, not a rule. A few did not: the CheckSystem
auxiliary collector and the Gearman job runner had no top-level catch at all,
and several places caught `std::exception` but not everything. Starting a
thread now comes with the guard, so such a failure is contained and logged and
costs one piece of work rather than the service.

Three things change in ways worth knowing about:

* **A worker that dies now leaves the agent up in a reduced state.** It keeps
  answering everything else, but whatever that thread was doing has stopped -
  a collector that dies takes its checks' data with it, and they go stale
  while the agent looks healthy. If a disappearing process was your signal (a
  systemd/SCM restart, a "service is down" alert), it no longer fires for this
  class of failure. Alert on the log instead; every one of these reports reads

    ```
    Thread '<name>': terminated by an uncaught exception: <details>
    ```

    with `<name>` naming the worker (`checkdisk collector`, `scheduler worker
    100`, `socket server worker 0`, …). Match on that text rather than on a
    severity - modules log it as `critical` and the core logs it as `error`.

* **A scheduler worker that dies is no longer silent.** The thread pool behind
  scheduled checks used to catch and discard the exception, leaving the pool
  quietly short of workers with nothing in the log to say why. It reports now.

* **Crash reports land next to your log file.** `nsclient.fatal` - the file the
  agent writes to when it is going down and cannot use the normal log - is
  written to the same folder as `nsclient.log` (`${log-path}`) instead of
  whatever directory the service happened to be started from. The folder is
  created if it is not there when a report has to be written, and if it cannot
  be written to (a read-only path, a full disk) the report goes to a private
  folder under the system temp directory rather than being lost:
  `%TEMP%\nsclient++\` on Windows, `/tmp/nsclient++-<uid>/` on Linux. The
  startup log says which file is in use. If an agent has been dying without
  explanation, that file is now the first place to look.

  Only the service does this. `nscp` command line invocations keep the old
  behaviour, so running one under `sudo` cannot leave a root-owned log folder
  behind for the service account to trip over.

See the [security notice](../security/notices.md#an-uncaught-exception-on-a-worker-thread-could-terminate-the-agent).
