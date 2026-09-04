**Default check (one record per interface):**

The defaults threshold total throughput: `throughput > 10000` warns and
`> 100000` is critical, in bytes per second.

```
check_network
OK: eth0 >659B/s <659B/s, ifb0 >0B/s <0B/s, ifb1 >0B/s <0B/s, lo >0B/s <0B/s|'eth0'=1318Bps;10000;100000 'ifb0'=0Bps;10000;100000 'ifb1'=0Bps;10000;100000 'lo'=0Bps;10000;100000
```

The message renders the human-readable `sent_human` / `received_human`; the
performance data carries the raw `throughput` in bytes per second.

**Exclude loopback and virtual interfaces:**

```
check_network "filter=name not like 'lo' and name not like 'ifb'"
OK: eth0 >659B/s <659B/s|'eth0'=1318Bps;10000;100000
```

Do this before setting any fleet-wide threshold — a container host has a lot of
`veth`/`docker` interfaces that will otherwise each become a record and a
performance-data series.

**Alert when a link is not up:**

Note that a loopback interface reports `unknown` rather than `up`, so filter it
out or this will fire on every host.

```
check_network "crit=link_status != 'up'" "warn=none" "detail-syntax=${name}: ${link_status}"
CRITICAL: eth0: up, ifb0: down, ifb1: down, lo: unknown
```

**Alert on interface errors rather than volume:**

Errors and drops are a cabling, driver or duplex-mismatch signal, and are
invisible to a throughput threshold. They are cumulative counters since boot, so
alert on any increase from your own baseline rather than on an absolute number.

```
check_network "crit=rx_errors > 0 or tx_errors > 0" "warn=none" "detail-syntax=${name}: rx_err=${rx_errors} tx_err=${tx_errors}"
OK: eth0: rx_err=0 tx_err=0, ifb0: rx_err=0 tx_err=0, ifb1: rx_err=0 tx_err=0, lo: rx_err=0 tx_err=0|'eth0_rx_errors'=0;0;0 'eth0_tx_errors'=0;0;0 'ifb0_rx_errors'=0;0;0 'ifb0_tx_errors'=0;0;0
```

**Threshold on link utilisation instead of raw bytes:**

`usage_in` / `usage_out` / `usage_total` are percentages of the link speed, which
ports across differently-sized links. They are `0` whenever `speed_bps` is
unknown — as it is on virtual interfaces and in many VMs — so pair the threshold
with a `speed_bps > 0` guard rather than trusting a 0% reading.

```
check_network "warn=usage_total > 60" "crit=usage_total > 85" "detail-syntax=${name}: ${usage_total}% of ${speed_bps}bps"
OK: eth0: 0% of 0bps, ifb0: 0% of 0bps, ifb1: 0% of 0bps, lo: 0% of 0bps|'eth0_usage_total'=0%;60;85 'ifb0_usage_total'=0%;60;85
```

**Inspect the raw fields:**

```
check_network "detail-syntax=${name} link=${link_status} rx=${received} tx=${sent} speed=${speed_bps}"
OK: eth0 link=up rx=659 tx=659 speed=0, ifb0 link=down rx=0 tx=0 speed=0, ifb1 link=down rx=0 tx=0 speed=0, lo link=unknown rx=0 tx=0 speed=0
```

**Right after the agent starts:**

The rates come from the 1 Hz background collector, so the first seconds after a
restart report no data rather than zeros.

```
check_network
UNKNOWN: No network data available yet (collector still initializing)
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_network --arguments "crit=rx_errors > 100"
OK: eth0 >659B/s <659B/s, lo >0B/s <0B/s
```
