---
icon: "📊"
modules: [filters]
action: conditional
---
**Performance data no longer reports a missing threshold as `0`.** When a
check's `warning` or `critical` expression did not bound a performance-data
keyword, the missing field used to be rendered as a literal `0`
(`'time'=2ms;1000;0`), which graphing tools such as PNP4Nagios, Grafana and
Icinga read as "critical above 0". The field is now left empty
(`'time'=2ms;1000`). A bound that really is `0` is still written as `0`.
Only if you parse the warn/crit fields of the perfdata yourself and relied on
the `0` placeholder do you need to treat an empty field as "no threshold".
