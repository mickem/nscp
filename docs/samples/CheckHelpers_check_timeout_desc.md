#### About `check_timeout`

`check_timeout` runs another check with a wall-clock deadline and returns
`Thread failed to return within given timeout` if the check has not answered in
time. It is available under the alias `timeout`.

The command to run is named with `command=` (`-q`) and its arguments are passed
one per `arguments=` (`-a`). `timeout=` (`-t`) is the deadline in seconds and
defaults to 30; `return=` (`-r`) overrides the status returned when the check
*does* finish in time.

Use it to stop a check that talks to something remote — a database, a share, an
HTTP endpoint — from hanging past your monitoring system's own timeout, which
would otherwise leave the service in a stale state with no message explaining
why.

Two things are worth knowing about how the deadline is enforced. On timeout the
worker thread is **detached, not killed**: the wrapped check keeps running to
completion in the background and only its result is discarded. A check that
hangs indefinitely therefore leaks a thread per invocation, so pair a short
timeout with a fix for whatever is hanging rather than treating it as a
permanent arrangement. And a timeout is reported as a *bad response* (UNKNOWN) —
`return=` only substitutes the status when the check finishes within the
deadline, so it cannot be used to make a timeout look like a success.
