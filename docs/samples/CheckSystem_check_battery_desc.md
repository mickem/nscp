#### About `check_battery`

`check_battery` reports the state of the machine's batteries: charge level,
which power source it is on, and battery health. One record is returned per
battery.

The default filter is `battery_present = 'true'`, and the thresholds are
`charge < 20` (warning) and `charge < 10` (critical). On a machine with no
battery — a desktop, a server, a VM — the filter matches nothing and the check
reports **`No battery found`** as its empty state rather than a false alarm.

##### The three questions it answers

**"Is this laptop about to die?"** is the default, and rarely the interesting
one on monitored infrastructure.

**"Is this machine on mains power?"** is often the real check.
`power_source` reads `ac`, `battery` or `unknown`, so a UPS-backed or
laptop-as-server host can alert the moment it drops to battery, long before the
charge level matters:

```
check_battery "crit=power_source = 'battery'" "warn=none"
```

**"Is the battery worn out?"** is what `health` is for — full charge capacity as
a percentage of design capacity. A battery at 60% health still charges to "100%"
and looks fine to a charge-level check while holding barely half its rated
runtime:

```
check_battery "warn=health < 70" "crit=health < 50"
```

`health` is emitted as performance data, so the decline is visible as a trend
long before it crosses a threshold.

##### Runtime and rates

`time_remaining` is the estimated seconds left, and is **`-1` when unknown or on
AC** — so guard any threshold on it with a `power_source = 'battery'` clause,
otherwise `time_remaining < 600` fires on every mains-powered host.
`charge_rate` and `discharge_rate` (mW) and the `design_capacity` /
`full_capacity` / `remaining_capacity` triple (mWh) are available for the
detailed view.

##### Data sources

On Windows the data comes from the Windows power/battery APIs. On Linux it is
read from `/sys/class/power_supply`, so batteries exposed by the ACPI or
platform driver are visible; a battery behind a vendor-specific driver that does
not populate sysfs will not be. `battery_status` carries the charging state
(`status` is a deprecated alias — the name clashes with the generic status
summary keyword and resolves to that in `top-syntax`).
