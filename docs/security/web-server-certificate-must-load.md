---
title: "WEB server: no fallback to cleartext HTTP when the certificate does not load"
fixed_in: next
severity: "Medium (affected setups only)"
modules: [WEBServer]
action: conditional
---
The WEB server decided whether to serve HTTPS by whether its certificate
*loaded*, and `allow insecure = false` was only enforced for a certificate
file that was *missing*. A certificate that was present but could not be
used fell through the gap:

- **Linux (beast backend):** an unreadable certificate or key file - wrong
  permissions after a renewal, a path to a directory - was logged and
  ignored, and port 8443 then served the REST API, login included, over
  plain HTTP. (A key that did not match the certificate did stop the server
  from starting.)
- **Windows (mongoose backend):** the listener came up and every TLS
  handshake failed.

Credentials and session tokens sent to such an agent travelled in clear, with
a single log line as the only sign.

The HTTP layer the WEB and NCPA servers share now checks that the certificate
and key parse and belong together, and a server whose certificate did not load
refuses to start rather than serve plain HTTP. The WEB server logs why. An
operator who really wants cleartext still removes the certificate and sets
`allow insecure = true`.

**What to do:** nothing on a working setup. If the WEB server no longer starts
after the upgrade, the log says which certificate or key could not be loaded:
fix or replace it (the default `${certificate-path}/certificate.pem` is
generated when missing).
