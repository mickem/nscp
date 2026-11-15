#### Optional mounts (`ignore-missing`)

By default a drive named with `drive=` that does not exist fails the whole
check:

```
check_drivesize drive=/data
Drive /data was not found
```

That is right when the mount is supposed to be there, and wrong when it is not
— a removable volume, a filesystem mounted only on some hosts of a group, a
share that is attached on demand. `ignore-missing=true` drops such drives
instead:

```
check_drivesize drive=/data ignore-missing=true
OK: No drives found
```

Drives that *do* exist are still checked normally, so mixing the two in one
call works and the missing one simply contributes nothing:

```
check_drivesize drive=/ drive=/data ignore-missing=true "warn=used > 90%" "crit=used > 95%"
OK All 1 drive(s) are ok|'/ used'=43.555GB;906.169;956.511;0;1006.854 '/ used %'=4%;90;95;0;100
```

**`ignore-missing=true` implies `empty-state=ok`.** Without that, a check whose
drives are *all* missing would report UNKNOWN — trading a false CRITICAL for a
false UNKNOWN, which still pages someone. Only the default is changed, so
asking for something else explicitly still wins:

```
check_drivesize drive=/data ignore-missing=true empty-state=warning
WARNING: No drives found
```

**On Windows, `require=` is unaffected.** Listing a drive there is an explicit
assertion that it is present, so it stays CRITICAL when absent even under
`ignore-missing` — which is the whole point of listing it. Use `drive=` +
`ignore-missing` for optional volumes and `require=` for mandatory ones; they
compose in one call.

The same option exists on [`check_files`](#check_files) (for scan paths) and
[`check_single_file`](#check_single_file) (for the file itself).

#### Time until full (`full_in`, `rate`)

Percent thresholds answer "how full is the disk", but a capacity alert is
really asking "how long until it *is* full" — a 4 TB volume at 91% may have
months left while a 10 GB volume at 70% has hours. The trend keywords
(`full_in`, `rate`, `trend_span`, `trend_samples`) answer that question
directly. Thresholds on `full_in` take duration literals:

```
check_drivesize "warn=full_in < 5d" "crit=full_in < 12h"
```

The estimate is an ordinary least-squares regression of used bytes over the
`trend-window` (default 24h), fed by the CheckDisk background collector which
keeps one sample per `trend interval` (default 5m) for `trend retention`
(default 7d) per drive — the same approach as Prometheus `predict_linear` and
Zabbix `timeleft`. History survives agent restarts (persisted hourly and on
shutdown at 30-minute granularity), and a filesystem resize discards the
now-meaningless history for that drive.

**Window choice is the sawtooth knob.** Over a window spanning several
cleanup cycles (log rotation etc.) the regression measures the *net* growth,
which is what capacity planning wants; a short window inside one cycle
reports the burst rate, which is what "something is filling the disk right
now" wants. Both are legitimate; pick per check:

```
check_drivesize "crit=full_in < 2h" trend-window=30m   # burst detector
check_drivesize "warn=full_in < 5d" trend-window=24h   # capacity planning
```

**`never` vs `unknown`.** `full_in`/`rate` are optional numbers: while the
drive is shrinking, flat, or has no usable history yet (fewer than 3 samples
or less than 3x the sampling interval of span — 15 minutes at the default
cadence), they simply have no value. A missing value satisfies *no* numeric
threshold (`full_in < 12h` is false on a shrinking disk, in both directions),
renders as `never` (`full_in`) / `unknown` (`rate`), and emits no perfdata.
`full_in = 'never'` matches exactly when there is no projection; use
`trend_span`/`trend_samples` to tell "no data yet" apart from "not growing",
e.g. `warn=trend_span < 1h`.

With `total=true` the total row reports the **minimum** `full_in` across the
matched drives (the soonest-full disk is the one that matters) and the sum of
their rates.

The collector can be tuned or disabled under `/settings/disk`:
`trend interval`, `trend retention`, and `trend` in the `disable` list
(disabling `disk_free` disables trends too, since they ride on the same
fetch). Without a collector the keywords stay at their no-value forms.

#### A sudden jump (`used_change_1h`, `used_change_24h`)

`full_in` and `rate` describe a trend; a runaway log, a dump or a backup
written to the wrong volume is a step, and the regression smooths it over.
`used_change_1h` and `used_change_24h` are the step itself: how many bytes of
used space the drive gained over the last hour and the last day, negative when
space was freed. Thresholds take size units:

```
check_drivesize "warn=used_change_1h > 2G" "crit=used_change_1h > 10G"
```

Both come from a second, small history the collector keeps per drive, in
memory only: one sample a minute for the last hour and one an hour for the
last day, spaced on a monotonic clock. A value is the latest sample's used
bytes minus those of the newest sample at least the window old — two real
readings, nothing interpolated — so the hour covers 60 to 70 minutes and the
day 24 to 26 hours. They describe the drive as of the collector's last sample
(10 seconds old at the default `collection interval`), not the instant the
check runs.

**Until a full window of history exists the value is `unknown`.** That is an
hour (a day) after the agent started, or after the drive appeared or changed
size — nothing is persisted across a restart. While it is `unknown` the
keyword fires no threshold in either direction and emits no perfdata, exactly
like `rate`; `filter=used_change_1h = 'unknown'` selects the drives that have
no value yet. A gap in the collector's sampling longer than the slack (10
minutes for the hour, 2 hours for the day) makes the value `unknown` again
rather than stretch it over a longer period than its name says. Without the
collector — a one-shot `nscp client` query, or `disable = disk_free` — both
are always `unknown`.

With `total=true` the total row adds up the changes of the drives that have
one. The same values are published as the `disk.free.<drive>.used_change_1h`
and `used_change_24h` metrics, which are absent (rather than 0) for as long as
the keywords are `unknown`.