---
title: "NCPA server: a new listener that runs queries for token holders"
fixed_in: next
severity: "Informational (new, off by default)"
modules: [NCPAServer]
action: conditional
---
The new, experimental `NCPAServer` module opens a network listener (HTTPS on
port 5693) on which anyone holding the NCPA token, from an address in
`allowed hosts`, can run the agent's queries over the `plugins/` node: every
registered check, alias and external script, unless `plugins` narrows it.
That is the same reach the NRPE server gives its callers, behind a shared
secret instead of a client certificate. The module is not loaded by default,
so an existing install is not affected until it is enabled.

How it is protected:

- **No default token.** Until `token` is set every request is refused. The
  token is compared in constant time, never logged and never echoed back;
  repeated wrong tokens from one address are blocked by the same rate limiter
  the WEB server uses.
- **No cleartext.** The listener refuses to start unless a certificate and
  key actually load (not merely exist), unless `allow insecure = true` is set.
  The token travels in the query string, so it is only as private as the
  connection.
- **No arguments by default.** `allow arguments = false` refuses a request that
  carries arguments, exactly as the NRPE server does; external scripts still
  apply their own `allow arguments` and `allow nasty characters` checks.
- **Narrowing.** `plugins = scripts` or a list of names limits what can be run;
  anything else answers exactly like a query that does not exist.
- **Separate from the WEB server.** Its own port, token and allow-list: the
  NCPA token opens nothing on the REST API, and WEB credentials open nothing
  here. Queries are attributed to `NCPAServer` in the core permission policy.

**What to do:** nothing unless you enable `NCPAServer`. If you do, set a long
random `token`, restrict `allowed hosts` to your Nagios servers, set
`bind to` if the listener belongs on one interface only, keep
`allow arguments = false` unless you need it, and consider `plugins = scripts`
or an explicit list.
