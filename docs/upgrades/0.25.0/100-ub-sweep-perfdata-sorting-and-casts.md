---
icon: "🔒"
modules: [filters, CheckHelpers, CheckSystemUnix, CheckDocker, CheckKubernetes]
action: none
---
**Undefined-behaviour sweep: whitespace-only perfdata parses as empty,
`filter_perf` sorts non-numeric entries last, and out-of-range numbers
saturate.** From the
[security notice](../security/notices.md#undefined-behaviour-sweep-perfdata-parsing-filter_perf-sorting-saturating-casts-and-script-bindings);
nothing to do, but three results can differ from before:

- Performance data consisting only of spaces no longer fails the check with
  an "out of range" error; it is treated as no performance data.
- `filter_perf sort=normal` and `sort=reverse` now place every entry without
  a numeric value after the numeric ones, in every case. Mixed lists used to
  come out in an order that depended on where the non-numeric entries sat.
- A floating-point keyword read as an integer, a `check_memory` /
  `check_pagefile` percentage threshold on Linux, and a Docker or Kubernetes
  JSON number beyond the 64-bit range now saturate at the integer limit
  instead of comparing against a wrapped or unspecified value.
