---
icon: "🔧 🔒"
modules: [NRPEServer, NRPEClient]
action: conditional
---
**`nscp nrpe install` writes `use ssl`, and the insecure preset's cipher string
says what it is.** The install command used to write `ssl = true`, a key the
server never reads: on a host where `use ssl = false` had been set previously,
it claimed encryption and client-certificate authentication while the listener
stayed in plaintext. Check that value if you ran the command on such a host.
The legacy (`insecure = true`) cipher default drops its `!ADH`, which never
excluded the anonymous elliptic-curve suites and could not have excluded them
without breaking a mode that loads no certificate; the string now matches what
`nrpe install --insecure` writes. The secure default gains `!aNULL` beside
`!ADH`, which changes nothing in practice — OpenSSL's default security level
already refuses anonymous suites. See the
[security notice](../security/notices.md#listeners-the-insecure-nrpe-cipher-string-a-key-nrpe-install-never-wrote-and-in-allowed-hosts).
