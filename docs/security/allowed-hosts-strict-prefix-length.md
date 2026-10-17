---
title: "`allowed hosts`: a malformed prefix length is refused instead of guessed"
fixed_in: next
severity: "Low"
modules: [NRPEServer, NSClientServer, NSCAServer, CheckMKServer, WEBServer]
action: conditional
---
The `/N` suffix of an `allowed hosts` entry was read by taking the first run of
digits anywhere after the `/` and ignoring everything else. A typo therefore
changed what the entry meant without a word in the log:

| Written                   | Was read as        | Effect                                      |
|---------------------------|--------------------|---------------------------------------------|
| `10.0.0.1/-1`             | `0.0.0.0/1`        | allowed `0.0.0.0`–`127.255.255.255`         |
| `10.0.0.0/abc24`          | `10.0.0.0/24`      | accepted a garbled entry as if it were valid |
| `10.0.0.0/255.255.255.0`  | `10.0.0.0/255`     | a dotted mask became a single host          |
| `10.0.0.1/33`, `::1/129`  | `/32`, `/128`      | an impossible prefix became a single host   |
| `10.0.0.1/`               | `/32`              | an empty prefix became a single host        |
| `/24`                     | *(skipped)*        | dropped without an error being logged       |

Only the first row widens access, and it needs a typo of an unusual shape, so
this is hardening rather than a bypass. The rest narrowed the list or kept it
as the operator probably meant, but still accepted something that is not a
prefix length.

The suffix must now be a `/` followed by decimal digits naming at most 32 bits
for an IPv4 address and 128 for an IPv6 one (leading zeros are fine). Anything
else is reported as `Invalid address: <entry> (the part after / must be a
prefix length: 0-32 for IPv4, 0-128 for IPv6)` and the entry is dropped; the
other entries in the list still apply. That is the same handling a malformed
`*` range already gets. An entry with nothing before the `/` is reported too.

For a host name the suffix is checked for shape before the name is resolved,
and its length against every address the name resolves to. If it is too long
for any one of them the whole entry is dropped: a name with both an A and an
AAAA record written as `name/40` is refused outright rather than kept as a
`/40` IPv6 range, since that is almost always an IPv4 typo.

**What to do:** check the log after upgrading for `Invalid address:` lines
naming an `allowed hosts` entry. Rewrite a dotted mask as a prefix length
(`10.0.0.0/255.255.255.0` → `10.0.0.0/24`). An entry that is dropped no longer
allows anything, so a host whose only entry was malformed now refuses every
connection until it is corrected.
