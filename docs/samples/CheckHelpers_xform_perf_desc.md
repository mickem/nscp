#### About `xform_perf`

`xform_perf` runs another check and **transforms its performance data** before
returning it. It exists for graphing backends that need something the original
check does not emit. The wrapped check's status and message pass through
untouched.

##### `mode=minmax`

Sets `min=0` and `max=100` on every **percentage** counter — those whose unit is
`%`. Counters with any other unit are left alone. Some graphing systems will not
draw a percentage gauge on a fixed 0–100 axis, or will autoscale it to the
observed range, unless the bounds are declared.

##### `mode=extract`

Copies one field of every counter into a **new, additional** counter, renamed
with `replace=`. The original counters are kept, so the result carries both
series and the graph can show a value against its own bound.

`replace=` is written as `<match>=<replacement>` and is a plain substring
substitution on the counter label, applied everywhere it occurs — so
`replace=used=size` turns `/ used` into `/ size` and `/ used %` into `/ size %`.
It must contain exactly one `=`, or the check returns a syntax error.

**Only `field=max` and `field=min` do anything.** Despite what the option help
suggests, `value`, `warn` and `crit` add no counters at all — the check simply
returns the original performance data unchanged, with no error. If an `extract`
run appears to be a no-op, this is why.

So the useful shape is emitting a counter's declared maximum as its own series:

```
xform_perf command=check_drivesize mode=extract field=max "replace=used=size"
```

An unrecognised `mode=` is an error (`Invalid mode specified`, UNKNOWN), not a
silent pass-through.

See also [`render_perf`](#render_perf), which turns performance data into the
message, and [`filter_perf`](#filter_perf), which sorts and trims it.
