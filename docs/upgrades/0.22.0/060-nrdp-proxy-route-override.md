---
icon: "🔒"
modules: [NRDPClient]
action: conditional
---
**A request may no longer reroute a credentialed NRDP target through its own proxy.**
The client host-override guard now treats `proxy=` and `no-proxy=` as moving
the request, since a caller-chosen proxy receives it whole, configured token
included (see the
[security notice](../security/notices.md#nrdp-a-request-supplied-proxy-sent-the-configured-token-through-a-caller-chosen-host)).
Nothing to do unless callers pass `proxy=` on `submit_nrdp` against a target
that carries a token; repeating the configured proxy is still fine, and the
remedies are those of `host=` — pass the token with the request, configure
the proxied route as its own target and select it with `target=`, or set
`allow host override = true`.
