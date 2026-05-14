#### Choosing a mode

`check_network` collects throughput counters from one of two WMI classes.
Which one to use is controlled by the `mode=` argument:

* `interface` (default) — Counters are read from `Win32_PerfRawData_Tcpip_NetworkInterface`.
  This is the historical behaviour and reports one row per *physical* network
  interface. NIC team aggregates (the virtual adapter that represents the team
  as a whole) are **not** visible here, only the underlying physical adapters.
  Use this mode unless you specifically need team statistics — it preserves the
  output that existing dashboards and thresholds were built against.

* `adapter` — Counters are read from `Win32_PerfRawData_Tcpip_NetworkAdapter`.
  This superset also reports the aggregated team adapter (e.g. a row called
  "Production Network" alongside the physical team members), which is what you
  want when monitoring traffic across an LBFO/Switch-Embedded team rather than
  individual ports. Note that the friendly Windows adapter name is used here,
  so interface names may differ slightly from `interface` mode for the same
  physical NIC.

* `both` — Every adapter is reported twice, once from each source. The `source`
  filter keyword (`source = 'interface'` or `source = 'adapter'`) can then be
  used inside warning/critical/filter expressions to distinguish them. This
  mode is mainly useful when you want to alert on the team aggregate *and* the
  individual members from one check.

A `source` filter keyword is available in every mode (its value is the literal
string `interface` or `adapter`), so you can write expressions such as
`source = 'adapter' and total > 100000000` to scope thresholds to a particular
source.

##### Identifying teamed adapters

Team aggregates only have perfraw data; they have no matching
`Win32_NetworkAdapter` row, so `MAC`, `speed`, `enabled` and
`net_connection_id` are empty for them. You can identify them with
`MAC = ''` in a filter expression when running in `adapter` or `both` mode.

#### Link speed and percent-usage values are best-effort

The `speed`, `speed_bps`, `usage_in`, `usage_out` and `usage_total`
variables all derive from the Windows `Win32_NetworkAdapter.Speed`
property. That property is the **negotiated link speed**, which is not
always the same thing as the actual usable throughput. The check reports
what Windows tells it; it does not measure the link.

**When the reported speed is unreliable or missing:**

* **Virtual adapters** (VPN tunnels, loopback, Hyper-V vNICs, some VMware
  paravirtual NICs) frequently report `Speed` as "Unknown" or empty. The
  check stores `speed_bps = 0` for those.
* **NIC team aggregates** (visible in `mode=adapter` or `mode=both`) may
  report `0`, `~0ULL`, or an arithmetically inconsistent value depending
  on the driver and team mode (LACP vs switch-independent vs static).
  Sometimes the team aggregate's Speed is the *sum* of member-link speeds;
  sometimes it is a *single* member-link's speed.
* **Wireless adapters** typically report the negotiated PHY rate (for
  example 866 Mbps for 802.11ac). Real-world throughput is usually
  40-60% of that because of MAC overhead, retransmits and rate adaptation,
  so a saturated wireless link may read as ~50% in `usage_*` rather than
  the 100% you'd expect.
* **Drivers that report a stale value** during link renegotiation can
  briefly show the wrong rate immediately after a cable change or
  speed switch.

**Variables affected by this:**

| Variable      | Best-effort behaviour when Speed is unknown                 |
|---------------|-------------------------------------------------------------|
| `speed`       | Raw string from WMI - may be `"Unknown"` or empty           |
| `speed_bps`   | Reads as `0` (the "unknown" sentinel)                       |
| `usage_in`    | Reads as `0` - indistinguishable from a genuinely idle link |
| `usage_out`   | Reads as `0` - indistinguishable from a genuinely idle link |
| `usage_total` | Reads as `0` - indistinguishable from a genuinely idle link |

The byte-rate variables (`received`, `sent`, `total`) and their
`*_human` companions are **not** derived from `Speed` and are unaffected
by these caveats. They come straight from
`Win32_PerfRawData_Tcpip_NetworkInterface` / `NetworkAdapter` cumulative
counters.

**Writing reliable percent-based alerts:**

The `0`-when-unknown sentinel was chosen so dashboards and `<`-style
alert rules behave naturally without special-casing. The trade-off is
that an unknown-speed link looks identical to a genuinely idle one. If
you need to distinguish them, filter on `speed_bps > 0` *before*
applying the percent threshold:

```
check_network "filter=speed_bps > 0" \
              "warning=usage_total > 80" \
              "critical=usage_total > 95"
```

For environments where percent thresholds are not viable (mixed wireless,
heavy NIC-team use, lots of virtual adapters), prefer absolute byte-rate
thresholds against `received`/`sent`/`total`, scoped to specific
interfaces by name:

```
check_network "filter=name = 'Ethernet 1'" \
              "warning=total > 800000000" \
              "critical=total > 950000000"
```

Both styles can be combined in a single check by using `filter` to scope
which interfaces participate, then `warning`/`critical` to set the
threshold.
