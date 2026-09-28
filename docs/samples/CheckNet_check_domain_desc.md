Checks the expiration of a registered domain using RDAP over verified HTTPS.
The command is experimental. By default it warns below 30 days and becomes
critical below 10 days, including domains that have already expired.
`expires_in` is a count of whole 24-hour periods rounded down, with negative
values after expiration. It is also emitted as performance data in days.

#### RDAP lookup

Supply the registered domain, such as `example.co.uk`, rather than a web or mail
host under that domain. Names are lowercased and a trailing dot is removed.
Internationalized names must be supplied in ASCII/punycode form (`xn--...`).
The check does not guess the registered domain from a hostname.

The default endpoint, `https://rdap.org/domain/{domain}`, redirects to the
appropriate RDAP service. `rdap-url` can override it with a provider's HTTPS
domain lookup URL; `{domain}` is replaced with the normalized name. Up to five
redirects are followed per record, relative paths are normalized using RFC 3986
dot-segment removal, and HTTPS-to-HTTP downgrades are rejected.
Certificate chain and hostname verification are always enabled. `ca` selects
a custom trust bundle; by default the module uses the system CA bundle.
Connections are direct; this check does not use an HTTP proxy.

Only the requested domain's own events are examined. A `registrar expiration`
event takes precedence over `expiration`. When only a registry date is present,
one domain-level related RDAP link is followed to look for the registrar date.
The output identifies the date as `registrar` or `registry`, and `source` names
the server URL that supplied it. If the registrar record is available but has
no registrar expiration event, the registry date is retained. Failure to fetch
or validate an advertised registrar record is a lookup failure, not a silent
return to the registry date.

Registry auto-renewal can advance the registry date even when the registrant
has not renewed. A registry date alone therefore does not prove renewal or
payment. Expiration is also not an exact prediction of when DNS or mail stops:
grace periods and suspension policies vary by provider.

Malformed responses, conflicting dates, a different domain in the response,
missing expiration, HTTP errors (including 404 and 429), TLS failures and
timeouts return UNKNOWN without expiration performance data. A 404 does not
prove that a domain is expired. `timeout` (default 15 seconds, maximum 300)
applies separately to each network read or write through the shared client.
There is no overall lookup deadline: redirects, registrar requests and a server
that continues sending data can extend the total duration. DNS resolution and
TCP connection establishment use the shared client's existing behavior.
Response bodies are limited to 1 MiB and HTTP headers to 16 KiB. HTTP message
framing is validated before the JSON is parsed: read failures, truncated
Content-Length bodies and incomplete chunked responses are lookup failures,
even if the bytes received so far contain valid JSON. Complete framed responses
return without waiting for the server to close the connection.

#### Optional WHOIS fallback

`whois-fallback=true whois-server=<server>` enables fallback after a failed
RDAP lookup. WHOIS uses TCP port 43 unless `whois-port` overrides it. Each
read or write uses the same `timeout` setting as RDAP. Successful fallback is
identified by a `whois:` source.
The check does not discover WHOIS servers or follow referrals.

WHOIS is unencrypted and unauthenticated, and many providers no longer offer
it. Enabling fallback also permits its use after a TLS verification failure.
Choose a server that you trust and that publishes the required domain data.

Supported fields are `Registry Expiry Date`, `Registrar Registration Expiration
Date`, `Expiry Date`, `Expiration Date`, `paid-till`, `expires` and `Expiration
Time` (case insensitive). Values must be RFC 3339 timestamps with explicit
timezones or ISO `YYYY-MM-DD` dates, interpreted as midnight UTC. Ambiguous
locale-dependent dates are rejected. Other WHOIS formats return UNKNOWN;
this is intentionally not a universal WHOIS parser.

#### Scheduling

Run this check daily from one monitoring location per domain. It makes live
requests on every invocation and does not cache responses or retry rate limits.
