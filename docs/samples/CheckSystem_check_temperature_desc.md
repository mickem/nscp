#### About `check_temperature`

`check_temperature` reports thermal sensor readings, one record per zone or
sensor, in degrees Celsius. The defaults are `temperature > 70` (warning) and
`temperature > 90` (critical).

##### Sensor availability is the main caveat

Thermal sensors are hardware- and driver-dependent, and a great deal of the
infrastructure this agent runs on does not expose any:

- **Virtual machines** almost never present thermal zones — the hypervisor owns
  the hardware.
- **Cloud instances** likewise.
- On Linux, readings come from the kernel's thermal zones and hwmon sensors
  (`/sys/class/thermal`, `/sys/class/hwmon`), so a sensor needs a loaded driver
  to appear. `sensors-detect` from `lm-sensors` is the usual way to find out
  what a given box can report.
- On Windows, ACPI thermal zones are read through WMI, and many vendors expose
  either nothing or a single coarse zone rather than per-component sensors.

Because of that, `empty-state` matters: the check reports OK with
*"All thermal zones seem ok."* when nothing matched. If a missing sensor should
itself be an alert on hardware you know has one, set `empty-state=critical`
explicitly.

##### Naming is not portable

`name` is whatever the platform calls the zone — `thermal_zone0`,
`coretemp Package id 0`, `TZ00` — and it differs between machines, vendors and
kernel versions. Do not hard-code a sensor name in a fleet-wide check; threshold
across all of them and use `detail-syntax` to identify the offender in the
message.

A sensible fleet-wide shape is a generous threshold on everything, since the
absolute numbers vary a lot between a CPU package sensor and a chassis sensor:

```
check_temperature "warn=temperature > 75" "crit=temperature > 90" "detail-syntax=${name}=${temperature}C"
```

`active` reports whether the zone is currently active; on Windows
`throttle_reasons` carries the ACPI throttle bitmask, which is a more direct
signal that thermal limits are actually biting than the temperature alone.
