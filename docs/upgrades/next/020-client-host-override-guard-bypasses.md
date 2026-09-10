---
icon: "🔒"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
**The client host-override guard now also covers a credential kept inside the
target's address.** Two ways past the 0.19.0 guard are closed (see the
[security notice](../security/notices.md#two-ways-past-the-client-host-override-guard)):
a secret carried in the URL — `address = https://h/submit.php?token=SECRET` or
`https://user:password@h/` — now counts as the configured credential it is, and
a target that supplies a credential but no address no longer adopts the
caller's `host=` as its own. Nothing to do unless you relied on one of those to
point a credentialed target at several hosts; the remedies are unchanged — pass
the credential with the request, configure each destination as its own target
and select it with `target=`, or set `allow host override = true`.
