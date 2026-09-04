#### About `check_dns`

`check_dns` resolves a name and checks how long it took and what came back. One
record is returned per looked-up host, so `host=` can be repeated to probe
several names in one check.

By default it asks for an `A` record through the system resolver, warns when the
lookup takes longer than 1000 ms, and goes critical when `result` is anything
other than `ok`. `type=` selects the record type (`A`, `AAAA`, `MX`, `TXT`,
`CNAME`, `NS`, `SOA`, `PTR`) and `server=` sends the query to a specific
nameserver instead — which is what turns this from "can this host resolve
names?" into "is *that* nameserver answering correctly?".

The `result` keyword is what carries the verdict, and it distinguishes the cases
a plain success/failure boolean would flatten:

- `ok` — the lookup succeeded and, if expectations were given, matched.
- `not_found` — the name does not resolve (NXDOMAIN or an empty answer).
- `mismatch` — the name resolved, but not to what you pinned with
  `expected-address=` / `expected=`. This is the interesting one: a stale or
  hijacked record answers instantly and looks healthy to a check that only
  measures latency.
- `error` — the resolver or the queried server failed or timed out.

Pin expectations wherever the answer is supposed to be stable — public
A records, MX records, the reverse of a load balancer address — and leave them
off for names that legitimately move.

`norecursion=true` (RD=0) asks the server to answer only from its own
zones and cache, which is how you verify that an authoritative server is serving
a zone itself rather than proxying the answer. `address-family=` restricts the
transport used to reach the DNS server; it does not restrict the record type,
which is what `type=` is for.
