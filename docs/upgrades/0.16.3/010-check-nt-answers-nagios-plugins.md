---
icon: "🔌"
modules: [NSClientServer]
---
**`check_nt` (NSClientServer) answers the real nagios-plugins client again.**
Requests without a trailing newline used to hang until the client timed out
(`No data was received from host!`) — broken since 0.12.2. Remove any
client-side timeout/retry workarounds; no configuration change is needed.
If you expose this legacy endpoint, see the new guidance on securing it
(password, `allowed hosts` and the `allow` command list).
