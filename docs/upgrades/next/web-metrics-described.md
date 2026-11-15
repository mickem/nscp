---
icon: "📊"
modules: [WEBServer, CheckDisk]
action: none
---
**New: the web UI's Metrics page says what each metric means.**
The page now reads `/api/v2/metrics?meta=1` and shows, next to every value,
what its producer declared about it: a description column with the help text,
its labels and its kind (`gauge`, `counter`, `info`) - the same description
`/api/v2/openmetrics` carries. Metrics are grouped under one heading per
module, type and instance (`disk › free › C:`) instead of repeating those on
every row, and a value is shown with its unit: a byte count is scaled
(`640.5 GiB`), a percentage reads `34 %`, and the exact number is in the
value's tooltip. The search field matches the help text, unit and labels as
well as the key.

CheckDisk's `free_pct` and `used_pct` now declare `percent` as their unit so
they read `34 %` there too. Their OpenMetrics families keep their names
(`disk_free_free_pct`): a percentage whose name ends in `_pct` is not given a
`_percent` suffix.

The module buttons now select a module exactly instead of searching for its
name, and metrics published by a script (`PythonScript`'s `fetch_metrics`),
which live in a bundle with no name, get a button of their own, `(unnamed)`.
That button used to be blank and cleared the filter instead of applying it.
No key moved and the flat `/api/v2/metrics` document is unchanged. Nothing to
do.
