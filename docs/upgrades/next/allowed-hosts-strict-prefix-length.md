---
icon: "🔒 🔧"
modules: [NRPEServer, NSClientServer, NSCAServer, CheckMKServer, WEBServer]
action: conditional
---
**`allowed hosts` refuses a malformed `/prefix` instead of guessing what it meant.**
Nothing to do if every entry is a plain address, a host name, a `*` range or an
address with a `/N` prefix length. The suffix used to be read as the first run
of digits after the `/`, so `/-1` became `/1`, `/abc24` became `/24`, a dotted
`/255.255.255.0` became a single host and `/33` was quietly a `/32`. Such an
entry is now logged as `Invalid address: <entry> (the part after / must be a
prefix length: …)` and dropped, while the rest of the list still applies. The
same goes for a host name whose prefix is too long for any one of the addresses
it resolves to (the whole entry is dropped, not just that address), and an
entry with nothing before the `/` is now reported instead of skipped. A host
whose only entry was malformed refuses every connection until it is fixed.
Rewrite dotted masks as prefix lengths (`10.0.0.0/255.255.255.0` →
`10.0.0.0/24`). See the
[security notice](../security/notices.md#allowed-hosts-a-malformed-prefix-length-is-refused-instead-of-guessed).
