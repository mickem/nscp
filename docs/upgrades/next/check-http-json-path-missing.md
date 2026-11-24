---
icon: "📊"
modules: [CheckNet]
action: conditional
---
**`check_http json-path=`: a missing field no longer compares as 0, and perfdata keeps decimals.**
Only checks that use `json-path=` are affected.

- A path that is missing from the response (or is `null`, or a body that is not
  JSON) used to compare as `0`, so `crit=qlen < 1` fired whenever the field was
  absent. It now never matches a numeric threshold in either direction, renders
  as `missing`, and emits no perfdata for that check run. A value that is present
  but not a number (a string, an object) used to compare as `0` too, and now
  never matches a numeric threshold either. If you relied on an absent field
  alerting through `< 1` or `= 0`, test for it explicitly:
  `"crit=qlen = 'missing' or qlen < 1"`.
- Perfdata for a `json-path=` alias was rounded to an integer, so `0.73` was
  graphed as `0`. It is now emitted as a float. The label is unchanged
  (`<alias>_<url>`), so existing graphs keep their series and simply start
  showing the fractional values.
