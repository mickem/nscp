---
icon: "🔒 💥"
modules: [core]
action: none
---
**A single failing background task no longer takes the whole agent down.**
Nothing to do. Until now, an unexpected error inside one of the agent's
background threads - a collector gathering disk or CPU samples, a worker
handling an incoming check, the web server, the scheduler - stopped the entire
service. Every check on that host went stale because one of them hit one bad
sample, and on Linux the service simply disappeared without writing anything
anywhere. Those threads are now contained: the failure is written to the log
and only that one piece of work stops. Incoming requests are contained the
same way, so a request that fails is just a failed request - the listener
carries on answering the next one.

This changes what a failure looks like from the outside, in two ways worth
knowing about:

* **An agent that used to vanish now stays up in a reduced state.** It keeps
  answering everything else, but whatever that thread was doing has stopped -
  a collector that dies takes its checks' data with it, and they will go stale
  while the agent looks healthy. If you were relying on the process
  disappearing (a systemd/SCM restart, a "service is down" alert) as your
  signal, that signal is gone: alert on the log instead. The line always reads

    ```
    Thread '<name>': terminated by an uncaught exception: <details>
    ```

    Match on that text rather than on a severity - modules log it as
    `critical` and the core logs it as `error`, and it is the core ones (the
    listeners and the scheduler) that matter most.

* **Crash reports now land next to your log file.** `nsclient.fatal` - the
  file the agent writes to when it is going down and cannot use the normal
  log - is written to the same folder as `nsclient.log` (`${log-path}`)
  instead of whatever directory the service happened to be started from. The
  folder is created if it is not there when a report has to be written, and
  if it cannot be written to (a read-only path, a full disk) the report goes
  to the system temp folder rather than being lost: `%TEMP%\nsclient.fatal`
  on Windows, `/tmp/nsclient.fatal` on Linux. The startup log says which one
  is in use.
  If an agent has been dying without explanation, that file is now the first
  place to look.

See the [security notice](../security/notices.md#an-uncaught-exception-on-a-worker-thread-terminated-the-agent).
