**A host with no battery:**

The default filter is `battery_present = 'true'`, and the empty state is
`warning` — so a desktop, server or VM reports WARNING out of the box.

```
check_battery
WARNING: No battery found
```

**Which is almost never what you want on infrastructure:**

```
check_battery "empty-state=ok"
OK: No battery found
```

Set `empty-state=ok` (or `ignored`) on every host where a missing battery is
normal, or the check sits permanently in WARNING.

**Default check on a laptop (`charge < 20` warns, `< 10` is critical):**

```
check_battery
OK: BAT0: 87% (ac, charging)|'BAT0_charge'=87%;20;10;0;100 'BAT0_health'=92%;0;0;0;100
```

**Alert the moment the host drops off mains power:**

Often the real check on a UPS-backed or laptop-as-server host — long before the
charge level matters.

```
check_battery "crit=power_source = 'battery'" "warn=none"
CRITICAL: BAT0: 87% (battery, discharging)
```

**Alert on a worn-out battery:**

`health` is full charge capacity as a percentage of design capacity. A battery
at 60% health still charges to "100%" and looks fine to a charge-level check.

```
check_battery "warn=health < 70" "crit=health < 50"
WARNING: BAT0: 100% (ac, full)|'BAT0_health'=64%;70;50;0;100
```

**Threshold on remaining runtime — guard it with a power-source clause:**

`time_remaining` is `-1` when unknown or on AC, so an unguarded threshold fires
on every mains-powered host.

```
check_battery "crit=power_source = 'battery' and time_remaining < 600" "detail-syntax=${name}: ${charge}% ${time_remaining}s left"
OK: BAT0: 87% -1s left
```

**Inspect the capacity figures:**

```
check_battery "detail-syntax=${name} rem=${remaining_capacity} full=${full_capacity} design=${design_capacity} rate=${discharge_rate}"
OK: BAT0 rem=48120 full=55300 design=60000 rate=0
```
