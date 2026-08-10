#### Jitter

Beyond "does it answer" (`loss`) and "how fast" (`time`), `check_ping` reports
how *steady* the latency is:

| Keyword  | Description                                                                                 |
|----------|---------------------------------------------------------------------------------------------|
| `jitter` | Mean variation between the round trip times, in ms. **`-1` until `count` is 2 or more.**    |

Jitter is the variation *between* packets, so it needs more than one. **`count`
defaults to 1**, which leaves `jitter` at `-1`; raise it to measure:

```
check_ping host=gw.example.com count=10 "warn=jitter > 20" "crit=jitter > 50" "top-syntax=${list}" "detail-syntax=${host} rtt=${time}ms jitter=${jitter}ms"
```

`-1` is a safe "not measured" marker rather than a magic number: jitter is a
magnitude, so a real reading is never negative and cannot be confused with it.
Note that a threshold like `jitter > 20` is simply false at `-1`, so leaving
`count` at its default silently never alerts — set both together.

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
