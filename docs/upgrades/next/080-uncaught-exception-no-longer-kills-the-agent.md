---
icon: "🔒 💥"
modules: [core]
action: none
---
**An uncaught exception on a worker thread no longer takes the agent down.**
Nothing to do. Background threads - the collectors, the socket server pools,
the web server, the scheduler - now run behind a guard: an exception that
escapes is logged as a critical naming the thread and the exception, and only
that thread ends. A listener's event loop is re-entered instead, so a request
that throws fails as a request rather than stopping the server. Two things
follow that are worth knowing about:

* A crash that used to be a vanished service now shows up as a `CRITICAL`
  line in `nsclient.log` and a degraded agent that keeps answering everything
  else. Alert on the log line if you were relying on the process disappearing.
* The last-resort report file `nsclient.fatal` is now written next to
  `nsclient.log` (`${log-path}`) instead of in whatever directory the service
  was started from, and a terminate handler writes the exception type, its
  message and the thread into it before the process exits.

See the [security notice](../security/notices.md#an-uncaught-exception-on-a-worker-thread-terminated-the-agent).
