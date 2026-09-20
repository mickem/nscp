---
title: "Web server: browser hardening headers, TLS 1.3 on Linux, session tokens and handler errors"
fixed_in: 0.22.0
severity: "Low"
modules: [WEBServer]
action: conditional
---
Five findings from a review of the web server and the shipped UI. None is
remotely exploitable on its own; together they are the difference between a
browser that can defend the admin UI and one that cannot.

#### No hardening headers were sent at all

Responses carried Content-Type and cookies and nothing else. With the session
token in browser storage, a page able to frame the agent got an authenticated UI
to click-jack — the only thing stopping it was that modern browsers partition
storage for cross-site iframes, which is the browser defending itself rather
than the agent asking. The absence of a content security policy also left any
future injection unmitigated.

Every response now carries `X-Frame-Options: DENY`,
`X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer` and a content
security policy (`default-src 'self'; frame-ancestors 'none'; base-uri 'none';
object-src 'none'`, plus what the bundled UI needs), with
`Strict-Transport-Security` added when the connection is TLS. They are applied
where the response is written, so static files, API answers and error pages all
get them.

#### The REST API was TLS 1.2 only on every Linux build

The beast backend — which is what every DEB and RPM uses — built its TLS context
with asio's `tlsv12_server` method, which pins *both* ends of the version range
to TLS 1.2, so TLS 1.3 could never be negotiated. It also honoured neither
`tls version` nor `allowed ciphers`, which the NRPE and NSCA listeners have
always taken. Both settings now exist under `[/settings/WEB/server]` and are
honoured, with `tls version` defaulting to `1.2+`. A value this listener cannot
honour stops it starting rather than quietly leaving the library defaults in
place, since falling back is the opposite of what an operator narrowing the
setting asked for; `tls version = sslv3` is refused outright, because the
context excludes SSL 3.0 and a range pinned to it would start a listener that
completes no handshake at all. The mongoose backend drives TLS through its own
stack, which exposes neither knob; it logs that it is ignoring a value the
operator set rather than pretending otherwise.

#### Every Basic-auth request minted a persistent session token

`process_auth_header` created an eight-hour token on every request, not only on
the login routes — so each Icinga `check_nscp_api` poll made one. The store
evicts the oldest live entry once it holds 4096, so a host running 20 checks a
minute filled it in about three and a half hours and then evicted the operator's
UI session within minutes; any authenticated user of any role could do the same
deliberately with 4096 requests. Only the login routes issue a token now; every
other route authenticates without adding to the store.

#### The session credential was stored twice

The server set the bearer as an HttpOnly `token` cookie (and the user as `uid`)
on every authenticated response, and no request path ever authenticated from a
Cookie header. The credential was simply stored a second time, in a mechanism
that looked functional to whoever read the code next. The identity is now
request-scoped state that is never serialized, and no `Set-Cookie` is emitted.
The UI keeps its bearer in `sessionStorage` rather than `localStorage`, so it no
longer survives a browser restart or travels in a profile backup; a token left
in `localStorage` by an earlier build is cleared on logout.

#### Handler exceptions were echoed to the caller, and leaked the response

Any exception out of a request handler — an unavailable peer address, a
disengaged optional, a regex error — returned its `what()` text verbatim to a
client that need not have authenticated, and the response object allocated for
that request was never freed. The detail goes to the agent log now, the caller
gets a bare 500, and the response is owned for the duration.

#### `/api/v2/scripts/<runtime>` accepted any module name

The URL's first segment was used verbatim as both the permission suffix and the
module the request was executed against, so a role granted `scripts.*` rather
than a specific runtime could drive the add/delete/show/list verbs of any loaded
module implementing them; on PUT and DELETE the segment could be empty. It is an
allow-list now (`ext`, `py`, `lua`), and anything else is refused with 400
before the permission check. `lua` is mapped for the first time — the runtimes
listing has always advertised it while the mapping never translated it.

**What to do:** nothing required. If you front the web UI with a proxy that
embeds it in a frame, note that `X-Frame-Options: DENY` and
`frame-ancestors 'none'` now forbid that. A script that logs in through Basic
auth and then reuses the `key` from the response body should call
`GET /api/v2/login`, which is the route that issues one.
