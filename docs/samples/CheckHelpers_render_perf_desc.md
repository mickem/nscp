#### About `render_perf`

`render_perf` runs another check and turns its **performance data into the
message**. The wrapped check's own numbers become filterable records, so you can
list, filter and threshold them from the outside — useful when the check itself
does not expose the value you want to alert on in its message, and when you want
to see what a check is actually emitting.

The check to run is named with `command=`, and its arguments follow (as
`arguments=`, or simply positionally). Because it is a filter check, the full
`filter=` / `warning=` / `critical=` / `top-syntax=` / `detail-syntax=`
vocabulary applies over one record per performance counter.

The default `detail-syntax` renders one tab-separated row per counter — key,
value, unit, warning, critical, min, max — which makes the output easy to read
in a terminal and easy to paste into a spreadsheet. Set `remove-perf=true` when
the rendered message is the point and you do not also want the numbers
duplicated as perf data on the result.

`empty-state` defaults to `unknown`, so a wrapped check that emits no
performance data at all reports UNKNOWN rather than a misleading OK.

See also [`filter_perf`](#filter_perf), which sorts and trims performance data
while leaving the message alone, and [`xform_perf`](#xform_perf), which rewrites
the perf data itself.
