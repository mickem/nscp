Reports per-CPU-socket frequency, load and hardware inventory, sourced from the
`Win32_Processor` WMI class (one instance per physical socket).

There are no default warning/critical thresholds: modern CPUs legitimately clock
far below their maximum at idle, so a `frequency_pct` default would warn on every
idle machine. Use `load_pct` for a per-socket utilisation alert. WMI does not
always have a load sample ready: on such a cycle `load_pct` renders as
`no load sample`, compares false against every numeric threshold and emits no
perfdata (it is never a fabricated `0`, which is a valid idle reading);
`load_pct = 'no load sample'` is the presence test. The inventory
columns (`architecture`, `l2_cache`, `l3_cache`) make the check double as the
per-socket CPU hardware inventory; pin them to detect a re-imaged or migrated
box (`crit=architecture != 'x64'`).
