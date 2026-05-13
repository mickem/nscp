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
