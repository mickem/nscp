#### About `check_multi`

`check_multi` runs several checks in one round trip and returns the **worst**
status any of them produced, concatenating their messages and merging their
performance data.

Each check is given as one `command=` argument holding the whole command line —
the command name plus its arguments, quoted as a single token. `command=` may be
repeated; `arguments=` is a deprecated alias for it. `separator=` (default
`, `), `prefix=` and `suffix=` shape the combined message.

Status escalation follows the usual Nagios ordering — OK < WARNING < CRITICAL <
UNKNOWN — so one UNKNOWN check makes the whole result UNKNOWN. If any command
cannot be executed at all, the whole check fails rather than silently reporting
on the subset that ran.

Note that the checks run **sequentially**, so the total run time is the sum of
the parts; keep an eye on your monitoring system's check timeout when combining
several slow checks, and consider wrapping the slow one in
[`check_timeout`](#check_timeout).

The legacy alias `CheckMultiple` is accepted for backwards compatibility.
