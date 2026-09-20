---
icon: "🔧 🔒"
modules: [core]
action: none
---
**`allowed hosts` finally understands the `*` ranges it has always advertised.**
`192.168.1.*` means `192.168.1.0/24`, `192.168.*` means `/16`, `10.*` means
`/8`, and a bare `*` is every address. Until now such an entry threw out of the
address parser and — with the default `cache allowed hosts = true` — took the
whole module with it, so the listener never started. A `*` in the middle of an
address (`192.*.1.1`), or one combined with an explicit `/mask`, is reported as
a configuration error, and an unparseable numeric entry is now an error beside
the others rather than an exception. Nothing to do; if you have been avoiding
`*` because it stopped a listener, it works.
See the [security notice](../security/notices.md#listeners-the-insecure-nrpe-cipher-string-a-key-nrpe-install-never-wrote-and-in-allowed-hosts).
