#### Choosing the IP version (`address-family`)

Every network check in this module accepts an `address-family` argument that
pins which IP version it uses:

| Value  | Aliases              | Meaning                                                     |
|--------|----------------------|-------------------------------------------------------------|
| `any`  | `both`, `unspec`, `` | **Default.** Let the resolver choose (the historic behaviour). |
| `ipv4` | `4`, `v4`, `inet`    | Resolve and connect over IPv4 only.                          |
| `ipv6` | `6`, `v6`, `inet6`   | Resolve and connect over IPv6 only.                          |

Values are case-insensitive. Anything else is rejected with
`Invalid address-family: <value>` rather than silently falling back to `any` —
a typo must not quietly stop testing the family you asked for.

Supported by `check_ping`, `check_tcp`, `check_ssh`, `check_http`, `check_dns`
and `check_ntp_offset`.

On a dual-stack host the default leaves the choice to the resolver, so a check
that passes tells you *one* of the two stacks works, not which. Pinning the
family is what turns that into an assertion:

```
check_ssh host=srv.example.com address-family=ipv6
OK: localhost:22 ok in 1ms
```

```
check_http url=http://srv.example.com/health address-family=ipv6
OK: http://srv.example.com/health -> 200 ok (2B in 3ms)
```

Run the same check twice — once per family — to monitor both paths
independently. A host with no address in the requested family fails rather than
falling back:

```
check_tcp host=v6-only.example.com port=443 address-family=ipv4
CRITICAL: v6-only.example.com:443 refused in 0ms
```

For `check_dns` the flag selects how the **DNS server** is reached, which is
independent of the record `type=` being queried — you can ask an IPv6-reachable
server for an A record. When no `server=` is given and the type is A/AAAA (the
system-resolver path), it additionally restricts the answer to that family.

```
check_dns host=example.com server=2001:db8::53 address-family=ipv6
OK: example.com -> 10.1.2.3 (1) in 0ms [ok]
```

`check_dns` and `check_ntp_offset` were previously IPv4-only regardless of the
server address; they now open the socket in whichever family the server
resolves to, so an IPv6 DNS or NTP server is reachable at all.

#### IPv6 literals in URLs

`check_http` accepts a bracketed IPv6 literal, as required by RFC 3986:

```
check_http url=http://[::1]:8080/health
OK: http://[::1]:8080/health -> 200 ok (2B in 1ms)
```

The brackets are part of the URL syntax (an unbracketed `::1` is ambiguous with
the `host:port` separator) and are kept in the `Host:` header, while the `host`
keyword reports the bare address.

#### A note on `check_ping`

`check_ping` uses ICMP echo, and ICMPv4 and ICMPv6 are separate protocols
rather than two modes of one: with `address-family=ipv6` the check sends an
ICMPv6 echo request (type 128) on an ICMPv6 socket. Two consequences:

* The `ttl` field is not populated over IPv6. The IPv6 hop limit is only
  available through ancillary data the check does not request, so it reports
  `0` there instead of an invented value.
* Raw ICMP sockets need privileges (root / `CAP_NET_RAW` on Linux,
  Administrator on Windows) for both families, exactly as before.
