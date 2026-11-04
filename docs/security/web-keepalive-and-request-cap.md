---
title: "WEB server: kept-alive connections and a configurable request body cap"
fixed_in: 0.26.0
severity: "Low"
modules: [WEBServer, NCPAServer]
action: conditional
---
Two changes to what an unauthenticated peer can make the WEB server (and the
NCPA listener, which shares the HTTP layer) do, both before any credential
is checked:

- **Connections are kept alive.** The beast backend (every Linux package)
  answered every request with `Connection: close`; it now keeps a connection
  for the client's next request, as the mongoose backend on Windows always
  has. A peer that opens a connection and sends nothing, or sends a request
  and then nothing, holds a session and its coroutine for up to 30 seconds
  before the server closes it - the same wait the first request always had
  on this backend, now also between requests. Stopping or reloading the
  server closes such connections at once rather than waiting them out.
- **The request body cap is a setting.** `max request size` under
  `[/settings/WEB/server]` bounds what the server buffers for one request
  before the request reaches authentication. Its default, 3 MiB, is what the
  mongoose backend has always accepted, so on Linux the cap rises from
  beast's built-in 1 MiB. A request declaring more is answered
  `413 Payload Too Large` and nothing of its body is kept; the refusal is
  logged once per source address per minute, with the count of the ones left
  out, so a peer looping oversized requests cannot flood the log.

Neither widens what a request can reach: authentication, roles and the
`allowed hosts` filter are unchanged, and a refused body is never read into
memory beyond the receive buffer the backend always had.

**What to do:** nothing on a default install. A host that wants the old 1 MiB
bound on Linux sets `max request size = 1MiB`; one that accepts large uploads
raises it. See the [upgrade note](../setup/upgrading.md#unreleased).
