---
icon: "🔒"
modules: [filters, CheckSystemUnix]
action: conditional
---
**Undefined-behaviour sweep: overflowing `convert()` / `neg()` values are
errors, and multi-letter units scale the same everywhere.** From the
[security notice](../security/notices.md#undefined-behaviour-sweep-filter-arithmetic-collector-and-status-races-process-memory-log-truncation-and-the-http-settings-cache);
nothing to do unless you relied on one of these:

- A `convert()` or `neg()` whose result does not fit 64 bits
  (`size > convert('9000000', 't')`, `written > convert(1.5e17, 'w')`) is
  reported as an evaluation error on the check instead of comparing against a
  wrapped or unspecified value.
- `convert()` reads its unit from the first letter for whole numbers too, as
  it already did for fractions: `convert(2, 'min')` is 120 seconds and
  `convert(2, 'kb')` is 2048 bytes. Both used to leave the number unscaled.
- On Linux, `check_process` reports `error=Cannot read statm` for a process
  that exits while it is being read, instead of a random memory size.
