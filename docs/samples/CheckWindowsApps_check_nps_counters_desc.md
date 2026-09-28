Enumerates and reads counters from an installed NPS performance object. The
default `object` is `NPS Authentication Server`; select
`object=NPS Accounting Server` for accounting. On localized Windows, supply the
installed localized object name if the English name is unavailable. Counter and
instance names are those provided by Windows, allowing version-specific fields
without assuming that every server exposes the same names.

Two PDH samples are always collected, separated by `sample-ms` (default 1000),
so rate counters are measured. Missing objects, failed samples, and unavailable
values return UNKNOWN instead of being represented as zero. The IAS service must
be installed. An object with no values returns the configured empty state,
UNKNOWN by default.

Each record exposes an object, counter, instance, and formatted numeric value.
The label includes all three names to avoid performance-data collisions. Aggregate
instances such as `_Total` are included: select the aggregate or individual
clients with `filter`, and do not sum both. There are no default numeric thresholds
because counters have different units and meanings. Match a counter by name when
setting `warning` or `critical`. Lifetime totals are not converted into interval
counts; rate counters already carry the rate computed by PDH.

For arbitrary counter paths or long-term collector averages, use CheckSystem's
`check_pdh`. For accepted/rejected percentages and reason codes, use
`check_nps_auth`.
