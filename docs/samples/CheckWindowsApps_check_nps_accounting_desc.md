Counts discarded NPS accounting requests (Security event 6275) during `window`
seconds. An accounting discard is critical by default. NPS must be installed,
failure auditing must be enabled, and the agent must be able to query audit policy
and read the Security log. Unavailable or incomplete event data returns UNKNOWN.

Optionally specify the **current** accounting `log-file` to report its existence,
size and modification age. A missing specified file is critical. Set
`require-traffic=true` to also alert on an empty file or one older than `max-age`
seconds (default 600). Leave this disabled for quiet installations. Select the
current filename when logs rotate; the command does not guess rotation patterns.

Without `log-file`, `log_state` is `not_checked`, and file size/age are `unknown`
with no corresponding performance data. A healthy discard count alone does not
prove that accounting writes succeed. File inspection also cannot establish
write permissions, SQL sink health, or that a particular request was persisted.
For SQL accounting, use CheckMSSQL to check the configured destination and record
arrival; use CheckDisk for free space. The command does not write synthetic
accounting records or modify NPS logging configuration.

The event scan shares the authentication check's event limit and deadline. A
Windows API/access error is UNKNOWN; explicit missing/empty/stale file states are
evaluated by the filter. Custom thresholds replace the defaults.
