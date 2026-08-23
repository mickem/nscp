#### About `check_pending_reboot`

`check_pending_reboot` answers a question no single Windows API does: **is this
machine waiting for a reboot, and why?** A pending reboot is signalled
independently by several subsystems, so the check reads each one and reports the
union. This is the reliable way to catch servers that have applied updates but
will not finish patching until they restart.

All signals are read from the 64-bit registry view, so a 32-bit agent under
WOW64 still reads the native keys.

The default threshold is `warn=pending = 1` (WARNING whenever a reboot is
pending, no critical). Override it to escalate, to alert only on specific causes
(e.g. `crit=servicing = 1`), or to suppress the default with `warn=none`. The
check always returns a single aggregate row, so there is no empty state.

##### How long has the reboot been pending?

A reboot queued minutes ago by an update is expected; one still queued days
later is usually the actionable case. The `age` and `written` keywords expose
how long the reboot has been pending, e.g.
`crit=pending = 1 and age > 7d`. The time comes from the last-write time of the
Component Based Servicing / Windows Update registry key (each exists only while
its reboot is queued); when both are set the oldest one wins. Two caveats:

- Only those two signals carry a timestamp. A reboot signalled solely by a
  pending file rename, computer rename or domain join reports
  `age`/`written` as `unknown`, which never trips a numeric threshold
  (test for it explicitly with `written = 'unknown'`).
- The registry records the key's *last* write, not its creation: if servicing
  re-touches the key while the reboot is still queued, `age` restarts. Treat it
  as "pending at least this long since the last signal update".
