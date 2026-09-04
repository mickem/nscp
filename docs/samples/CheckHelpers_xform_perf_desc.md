#### About `xform_perf`

`xform_perf` runs another check and **transforms its performance data** before
returning it. It exists for graphing backends that need something the original
check does not emit.

**`mode=minmax`** fills in missing `min`/`max` bounds on each counter. Some
graphing systems will not draw a gauge, or will autoscale it badly, without
them.

**`mode=extract`** promotes one field of every counter — chosen with `field=`
(`value`, `warn`, `crit`, `min` or `max`) — to be that counter's value, and
renames the counter with `replace=`. The replace expression is written as
`<match>=<replacement>`: the matched part of the original label is substituted,
so `replace=used=used_crit` turns `c: used` into `c: used_crit`.

This is how you get a check's *thresholds* onto the same graph as its values:
run the check once normally, and once through
`xform_perf mode=extract field=crit` to emit the critical line as its own
series. Because the two runs are independent, pin the same thresholds on both
so the lines stay in step.

`extract` requires a `replace=` expression containing exactly one `=`; without
it the check returns a syntax error rather than silently emitting duplicate
labels.

See also [`render_perf`](#render_perf), which turns performance data into the
message, and [`filter_perf`](#filter_perf), which sorts and trims it.
