---
title: "RADIUS probe: authenticated replies and file-based credentials"
fixed_in: 0.24.0
severity: "Informational"
modules: [CheckNet]
action: none
---
The new experimental `check_radius` command validates the response identifier,
Response Authenticator, and Message-Authenticator before exposing a reply as
valid. It sends a fresh random request authenticator and Message-Authenticator,
and accepts responses only from the selected UDP endpoint. Servers that omit
Message-Authenticator do not pass the check, including older implementations
where the attribute was optional.

Shared secrets and test passwords are supplied through files rather than command
arguments. The check does not expose them in results. PAP password hiding does
not provide transport encryption, and the probe does not implement RadSec or EAP.

This is an additive command, not a fix for an existing NSClient++ vulnerability.
**What to do:** no upgrade action is required. When configuring the probe, protect
the credential files, use a limited test identity on a protected network, and
verify that the server returns Message-Authenticator.
