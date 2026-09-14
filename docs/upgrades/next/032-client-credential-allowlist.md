---
icon: "🔒"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient, NRPEClient]
action: conditional
---
**A request may no longer shape the connection a configured credential travels over.**
The host-override guard was a list of refused keys that grew by one security
notice at a time; it is now an allow list. While a target's own password or
token is what goes out, a request may set the payload, `timeout`, `retry`, a
credential it supplies itself and `target=`, and nothing else. The destination
and proxy keys were already refused; `verify=`, `ca=`, `tls-version=`,
`allowed-ciphers=`, `insecure=` and `encryption=` now are too, since they
decide how well the credential is protected on the way (see the
[security notice](../security/notices.md#a-request-can-no-longer-shape-the-connection-a-configured-client-credential-travels-over)).
Nothing to do unless your callers set one of those against a credentialed
target; repeating a value the target already configures is still accepted, and
the remedies are unchanged - pass the credential with the request, configure
the other destination as its own target and select it with `target=`, or set
`allow host override = true`.
