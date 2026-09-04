#### About `check_connections`

`check_connections` counts the host's TCP and UDP sockets and reports them as
one record per bucket. There is a bucket per protocol/family combination
(`tcp`/`tcp6`/`udp`/`udp6`), plus a `total` bucket that carries the per-state
breakdown.

The **default filter is `protocol = 'total'`**, so a bare call reports the one
aggregate row and thresholds `total_connections` at 1000 (warning) and 2000
(critical). Widen it (`filter=none`) to see the per-protocol buckets as separate
records.

The per-TCP-state counters — `established`, `listen`, `syn_sent`, `syn_recv`,
`time_wait`, `close_wait`, `closing`, `fin_wait`, `last_ack` and `udp` — live on
the `total` bucket only, and are emitted as performance data by default, so the
check graphs a full socket-state breakdown out of the box even with no
thresholds set.

Those states are what makes this more useful than a plain connection count:

- **`close_wait` climbing** is the classic application bug signal — the peer
  closed, the local process never called `close()`, so the socket is pinned
  until the process exits. It does not resolve on its own.
- **`syn_recv` climbing** means half-open connections are accumulating: a SYN
  flood, or a backlog the application is not accepting fast enough.
- **`time_wait` climbing** is usually benign on a busy server (sockets waiting
  out 2MSL), but a large plateau can exhaust ephemeral ports on a host that
  makes many short-lived outbound connections.

Because a healthy count is entirely workload dependent, baseline the host before
tightening the defaults, and prefer thresholding the specific state you care
about over the total.
