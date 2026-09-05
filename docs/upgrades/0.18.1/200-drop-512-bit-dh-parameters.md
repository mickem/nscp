---
icon: "🔒"
modules: [packaging, NRPEServer]
action: conditional
---
**The 512-bit Diffie-Hellman parameter file is no longer shipped.**
`security/nrpe_dh_512.pem` was installed alongside `nrpe_dh_2048.pem` and was
the default of an unused settings helper. Nothing in the agent used it — the
NRPE server has defaulted to `nrpe_dh_2048.pem` — but 512-bit DH is
Logjam-broken, and an operator copying the shipped default into `dh` inherited
it. The file and the dead default are gone. **If you set `dh` to
`nrpe_dh_512.pem` explicitly**, change it to `${nrpe-dh}/nrpe_dh_2048.pem`
before upgrading; otherwise the listener will fail to start with a missing DH
file. An existing copy on disk is left where it is by the upgrade. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
