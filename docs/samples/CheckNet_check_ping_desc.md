#### Jitter

Beyond "does it answer" (`loss`) and "how fast" (`time`), `check_ping` reports
how *steady* the latency is:

| Keyword  | Description                                                                                 |
|----------|---------------------------------------------------------------------------------------------|
| `jitter` | Mean variation between the round trip times, in ms. **`unknown` until `count` is 2 or more.** |

Jitter is the variation *between* packets, so it needs more than one. **`count`
defaults to 1**, which leaves `jitter` unmeasured; raise it to measure:

```
check_ping host=gw.example.com count=10 "warn=jitter > 20" "crit=jitter > 50" "top-syntax=${list}" "detail-syntax=${host} rtt=${time}ms jitter=${jitter}ms"
```

`jitter` is an *optional number*: until it can be measured it renders as
`unknown`, **every numeric comparison on it is false** (in both directions —
`jitter > 20` and `jitter < 20` alike), and no jitter perfdata is emitted. Test
for the unmeasured state explicitly with the string form:

```
check_ping host=gw.example.com count=10 "warn=jitter > 20 or jitter = 'unknown'"
```

Note that leaving `count` at its default means `jitter > 20` silently never
alerts — set both together, or add the `= 'unknown'` clause to catch it.

> **Upgrading.** `jitter` and `ttl` used to report `-1` when unmeasurable.
> Filters written against that sentinel (`jitter = -1`, `ttl != -1`) no longer
> match anything and must become `jitter = 'unknown'` / `ttl != 'unknown'`.
> Perfdata for an unmeasured value is now omitted rather than plotted as `-1`,
> so RRD-backed graphs will see the metric appear and disappear.

**A slow link is not a jittery one.** A host that consistently answers in 250 ms
has a large `time` and near-zero `jitter`; a host alternating between 10 ms and
200 ms has a small average `time` and large `jitter`. Latency-sensitive traffic
(VoIP, RDP, database replication) cares about the second far more than the
first, which is why they threshold separately:

```
check_ping host=voip-gw.example.com count=20 "warn=jitter > 30 or loss > 1%" "crit=jitter > 60 or loss > 5%"
```

**On the `total` row, `jitter` is the worst value across hosts**, not a jitter
computed over all the hosts' round trip times pooled together — mixing a fast
host with a slow one would manufacture a large number that describes nothing.
So a fleet-wide `crit=jitter > 50` fires when *any* host is that unstable:

```
check_ping hosts=a.example.com,b.example.com,c.example.com count=10 total=true "crit=jitter > 50"
```

Note that `time` remains the round trip time of the **last** reply, not an
average over the burst.

#### TTL

`ttl` and the `ttl=` argument are two different numbers that share a name, the
same way `ping -t` and the `ttl=` in its output do:

| Name              | Meaning                                                                        |
|-------------------|--------------------------------------------------------------------------------|
| `ttl=N` (argument)| TTL / hop limit stamped on the packets **we send**. `0` (default) keeps the system default. |
| `${ttl}` (keyword)| TTL of the **reply we got back** — what is left of the remote host's own outgoing TTL after the return path. |

```
check_ping host=router.example.com "top-syntax=${list}" "detail-syntax=${host} replied with ttl=${ttl}"
```

The reply TTL is a rough proxy for path length, so a drop in it means the route
changed — traffic failing over to a longer path, for instance:

```
check_ping host=peer.example.com "warn=ttl < 50" "crit=ttl < 20"
```

Limiting the outgoing TTL is how you check that a host is where you think it is
on the network: with `ttl=1` only a directly attached neighbour can answer.

```
check_ping host=gw.example.com ttl=1
```

`ttl` is **`unknown`** when no reply carried one — nothing came back, or the
check ran over IPv6, where the hop limit is not available without ancillary
data the check does not request. Like `jitter` it is an optional number: while
unknown it renders as `unknown`, every numeric comparison on it is false (so
`ttl < 20` will not fire on an unanswered host — use `loss` for that), no ttl
perfdata is emitted, and `ttl = 'unknown'` tests for the state directly.

On the `total` row `ttl` is the **lowest** value across hosts (the reply closest
to running out of hops), and hosts with no TTL are ignored rather than dragging
the fleet-wide value to "unknown".

#### Packet size

`size=N` sets the ICMP payload to exactly N bytes. The `payload` string is
repeated and cut to length, so the bytes on the wire stay recognisable rather
than being a run of zeroes. `size=0` (the default) sends the `payload` string
as-is, unchanged from previous behaviour.

The 8-byte ICMP header sits on top of the payload, and IPv4 adds 20 more, so
**1472 bytes is the largest payload that fits an untagged 1500-byte MTU**. That
makes `size` the tool for finding a path-MTU or fragmentation problem — a link
that passes small packets and silently drops big ones:

```
check_ping host=remote.example.com size=1472 count=5 "crit=loss > 0%"
```

The accepted range is 0–65507 (65535 minus the IPv4 and ICMP headers); anything
outside it is rejected with a message rather than being silently clamped.
