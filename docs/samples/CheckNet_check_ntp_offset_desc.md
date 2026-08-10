#### Is the clock wrong, or is the source unstable?

`offset` answers the first question. A source can answer promptly with a
believable offset and still be unusable, because that offset will not hold
still — that is what the remaining keywords are for.

| Keyword           | Description                                                                                      |
|-------------------|--------------------------------------------------------------------------------------------------|
| `jitter`          | RMS variation between the sampled offsets, in ms. **`-1` until `samples` is raised to 2 or more.** |
| `samples`         | How many samples actually answered.                                                               |
| `root_delay`      | Round-trip delay the server reports to its own reference clock, in ms.                            |
| `root_dispersion` | Maximum error the server claims for the time it serves, in ms.                                    |

`root_delay` and `root_dispersion` come straight out of the packet header, so
they need no extra traffic and are available from the default single query.
They are the server's own statement about its accuracy — useful for spotting a
source that has lost its upstream and is coasting on a free-running clock,
which it will happily keep serving:

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=${server} root_delay=${root_delay}ms root_dispersion=${root_dispersion}ms stratum=${stratum}"
OK: ntp.example.com root_delay=11ms root_dispersion=33ms stratum=2
```

#### Measuring jitter (`samples`)

Jitter is the variation *between* measurements, so it needs more than one.
**`samples` defaults to 1**, which sends a single query exactly as before and
leaves `jitter` at `-1`:

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=samples=${samples} jitter=${jitter}"
OK: samples=1 jitter=-1
```

Raise it to measure:

```
check_ntp_offset server=ntp.example.com samples=6 "warn=jitter > 50" "crit=jitter > 100" "top-syntax=${list}" "detail-syntax=${server} jitter=${jitter}ms over ${samples} samples"
WARNING: ntp.example.com jitter=70ms over 6 samples|'ntp.example.com_jitter'=70ms;50;100
```

`-1` is a safe "not measured" marker rather than a magic number: jitter is a
magnitude, so a real reading is never negative and cannot be confused with it.
Note that a threshold like `jitter > 50` is simply false at `-1`, so leaving
`samples` at its default silently never alerts — set both together.

Three things worth knowing about how the burst behaves:

* **Sampling stops at the first failure.** An unreachable or slow server costs
  one timeout, not `samples` of them, so raising `samples` does not multiply
  the worst-case runtime of the check.
* **The reported `offset` and `time` come from the quickest exchange.** A
  delayed packet biases the offset by roughly half its extra delay, so the
  fastest round trip is the most trustworthy estimate. With the default of one
  sample this is simply that sample.
* **A steady offset produces no jitter.** A clock that is consistently five
  seconds wrong is inaccurate but perfectly stable, so it shows a large
  `offset` and a near-zero `jitter`. The two conditions are independent and
  worth alerting on separately:

```
check_ntp_offset server=ntp.example.com samples=6 "warn=offset > 100 or jitter > 50" "crit=offset > 1000 or jitter > 200 or stratum >= 16" "top-syntax=${list}" "detail-syntax=offset=${offset_signed}ms jitter=${jitter}ms"
WARNING: offset=35ms jitter=70ms|'ntp.example.com_jitter'=70ms;50;200
```
