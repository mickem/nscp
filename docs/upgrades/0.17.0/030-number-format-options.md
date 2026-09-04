---
icon: "🔢"
modules: [filters]
action: none
---
**Check messages can now be told how to render their numbers.** Every filter
check gained four options - `decimals`, `byte-unit`, `decimal-separator` and
`thousands-separator` - so `check_drivesize` can report
`141.06GB/1006.85GB` (or `141,06GB/1.006,85GB`) instead of
`140.293GB/0.983TB`. The defaults are unchanged, so an installation that does
not set them renders exactly what it rendered before. The options only touch
the message: performance data keeps its full precision and its `.` radix, and
so do the numbers you write in a filter or a threshold. Real-time filters take
the same settings as `decimals`, `byte unit`, `decimal separator` and
`thousands separator` keys, inheritable from the default template. `decimals`
is capped at 15 (a `double` carries no more than that): the query option and
the `format_bytes()`/`format_number()` argument reject a larger value, and the
settings key clamps it, so a typo like `decimals=1000000` can no longer make a
check try to render a multi-megabyte number.
Note that setting **any** of the four options also moves plain float keywords
in the message onto the number format: with `decimals` unset they then render
with up to three decimals (trailing zeros stripped) instead of the legacy
6-significant-digit form — `2.71094` becomes `2.711`, and large values stop
rendering scientific (`1.23457e+07`). A pipeline that matches float text in
the message may need its pattern relaxed when you first set one of these
options; leave all four unset and the message is byte-for-byte unchanged.
