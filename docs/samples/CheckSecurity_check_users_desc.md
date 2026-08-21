#### About `check_users`

`check_users` reports the interactive logon sessions on the host — count and
per-session detail. It works on **both platforms**:

* **Windows** — via the Remote Desktop Services (WTS) API, so it distinguishes
  console from RDP and active from disconnected sessions (no WMI needed).
* **Linux** — via the utmp database (the same source as `who`); network logins
  (ssh) carry the remote host in `client`.

The built-in `count` summary variable holds the number of matching sessions.

There is **no default threshold** — this is a count/inventory check, so supply
your own, e.g. `crit=count > 10` or `crit=session_state = 'disconnected'`.
Sessions with no user (services, the RDP listener) are not counted.
