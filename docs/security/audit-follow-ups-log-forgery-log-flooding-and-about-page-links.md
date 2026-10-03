---
title: "Audit follow-ups: NRPE log escaping, check_nt log flooding and About-page link schemes"
fixed_in: next
severity: "Low"
modules: [NRPEServer, NSClientServer, WEBServer]
action: none
---
Three informational findings from the 2026-09 source review were closed. None
of them gives a caller more than it already had; each tidies up what a peer
inside `allowed hosts` could make the agent write or render.

- **NRPE logged the command name and arguments as raw wire bytes** on three
  paths: the trace line for every request, which ran before the metacharacter
  filter and carried the arguments too, the trace line for every response, and
  the error line for an internal exception. The core logger does not scrub
  control characters, so a command or argument carrying `CR`/`LF` could forge
  a second, convincing log line. All three now pass those fields through the
  same `escape_for_log()` that already protects the TLS peer name, which
  renders control characters as `\xNN` and truncates an oversized value.
- **check_nt logged an error for every request when no password was
  configured.** Every request is refused in that state, which is right, but
  an allowed host could grow `nsclient.log` by a line per packet for as long
  as it liked. The condition is now reported once, when the module loads, and
  the per-request refusal is a debug-level trace. Two argument-filter fields
  the server inherited from its NRPE sibling but never read were removed with
  it.
- **The About page of the web UI linked to each bundled npm package's
  homepage verbatim.** That field comes from the package's own metadata, and
  React only warns on a `javascript:` href, so a compromised transitive
  dependency could have planted one for an administrator to click. The
  license generator now accepts only an `http:` or `https:` URL that parses,
  and falls back to the package's npmjs.com page otherwise.

None of these is known to have been exploited.

**What to do:** nothing beyond upgrading. If you alert on NRPE log lines, note
that a command name or argument string is now rendered with `\xNN` escapes
for control characters and cut at 255 characters.
