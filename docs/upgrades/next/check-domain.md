---
icon: "🧪"
modules: [CheckNet]
action: none
---
**New `check_domain` in `CheckNet`: domain expiration over RDAP
(experimental).** Nothing to do on an upgrade. `check_domain domain=example.com`
reads the registered domain's expiration date over RDAP and, by default, warns
below 30 days and goes critical below 10 (`expires_in`, in whole days, also
emitted as performance data). Pass the registered domain (`example.co.uk`),
not a host under it; the check does not guess one from the other.

It makes a live request on every run, so schedule it daily from one location
per domain. The default endpoint is the public `https://rdap.org/domain/{domain}`
redirector, which means the domain name you check is sent to a third party and
the registry it redirects to. `rdap-url=` points it at a provider of your
choice. TLS verification is always on (`ca=` selects a trust bundle), redirects
from HTTPS to HTTP are refused, and the check connects directly, without an
HTTP proxy.

`whois-fallback=true whois-server=<server>` falls back to WHOIS on TCP port 43
when RDAP fails. WHOIS is unencrypted and unauthenticated, and the fallback
also applies after a TLS verification failure, so enable it only with a server
you trust. Malformed or conflicting answers, HTTP errors (including 404 and
429) and timeouts return UNKNOWN.
