---
icon: "📊"
modules: [WEBServer]
action: none
---
**New: the web UI's Metrics page says what each metric means.**
The page now reads `/api/v2/metrics?meta=1` and shows, next to every value,
what its producer declared about it: the help text under the metric's name,
its labels, its unit and its kind (`gauge`, `counter`, `info`) - the same
description `/api/v2/openmetrics` carries. The search field matches the help
text, unit and labels as well as the key.

The module buttons now select a module exactly instead of searching for its
name, and metrics published by a script (`PythonScript`'s `fetch_metrics`),
which live in a bundle with no name, get a button of their own, `(unnamed)`.
That button used to be blank and cleared the filter instead of applying it.
No key moved and the flat `/api/v2/metrics` document is unchanged. Nothing to
do.
