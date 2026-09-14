---
icon: "🔒"
modules: [ElasticClient, Op5Client, IcingaClient, core]
action: none
---
**An Elastic or Op5 submission over an unverified `https` link now says so in
the log.** When the endpoint's `verify mode` resolves to no peer verification
and credentials are configured, both modules log a message naming the endpoint
— the credentials then go to whichever server answers — matching what the
Icinga, NRDP and NRPE clients already do. It is logged once per module load,
and the connection itself is unchanged. The Icinga client's own warning was
repeated on every submission and is now logged once per target instead. The
shared `--verify`, `--ca`, `--certificate-key` and `--allowed-ciphers` help
texts (`NRPEClient`, `NSCPClient`, `NSCAClient`, `CheckMKClient`,
`NSCANgClient`) were corrected at the same time: three of them read "Client
certificate format". See the
[security notice](../security/notices.md#elastic-and-op5-submissions-warn-about-an-unverified-tls-link).
