---
icon: "🔒"
modules: [CheckWindowsApps, CheckNet]
action: none
---
**NPS and RADIUS monitoring commands.** `CheckWindowsApps` adds experimental
`check_nps_auth`, `check_nps_accounting`, and `check_nps_counters` commands.
`CheckNet` adds experimental `check_radius` for PAP authentication, expected
rejection, or explicitly enabled Status-Server probes. Existing checks and
configuration remain unchanged. The RADIUS probe requires protected credential
files and validates both response authenticators; see the
[security notice](../security/notices.md#radius-probe-authenticated-replies-and-file-based-credentials).
