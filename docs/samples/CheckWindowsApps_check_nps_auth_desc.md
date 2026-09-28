Summarizes local NPS Security audit events in a fixed window ending when the check
starts. `window` is in seconds (default 300). Events 6272, 6273 and 6274 represent
accepted, rejected and discarded authentication requests. XML field names are
parsed directly; localized rendered messages are not used.

The default output is one `all` record, including a quiet window with zero events.
`group-by=client`, `policy` or `reason` produces one record per observed group.
Missing grouping fields are reported as `unknown`. A group absent from the window
cannot be detected without an expected-client inventory; grouped checks therefore
return the configured empty state when no groups are found.

`reject_pct` is `100 * rejected / (accepted + rejected)`. Discards are reported
separately and excluded from that denominator. With no decisions the percentage
is zero; inspect `decisions` or `requests` to distinguish this from successful
traffic. Default percentage alerts apply only after `min-requests` decisions per
group (default 20): warning above 10%, critical above 25%. Any discarded request
is critical by default. `top_reason` is the most frequent numeric reject/discard
reason code, with lexical tie-breaking, or `none` without failures.

Use `require-traffic=true` only where authentication traffic is expected. It adds
a default critical condition for zero requests and requires `group-by=all`.
Custom warning/critical expressions replace the defaults, including their
minimum-volume and traffic guards.

The IAS service must be installed, the agent must be able to query audit policy
and read Security events, and NPS success **and** failure auditing must be enabled.
Missing prerequisites return UNKNOWN even if `empty-state=ok` is requested.
Enable the Network Policy Server audit subcategory through local/group policy.
Run `check_service service=IAS` separately to monitor service state.

The scan is bounded by `max-events` (default 100000) and a 15-second enumeration
deadline. A failed, malformed or truncated scan returns UNKNOWN, never partial
counts. Windows query setup itself may take additional time. The check counts
available audit records; it cannot reconstruct cleared/overwritten logs or events
from periods when auditing was disabled. It does not maintain bookmarks or
represent every packet received at the UDP listener.
