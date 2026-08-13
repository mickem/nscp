#### About `check_mysql_query`

`check_mysql_query` runs an arbitrary SQL query and applies thresholds to the
returned rows, CheckWMI-style: every column of the result set is registered as
a filter keyword, so `filter=`, `warning=` and `critical=` expressions can
reference columns by name, and `detail-syntax` can render them with
`%(column)`. The built-in `line` keyword renders a whole row as
`column=value` pairs.

Notes:

* Each row of the result set is matched separately, so a query returning one
  row per database/queue/job gives per-item results and `${problem_list}`
  works as usual.
* Columns are compared numerically when the threshold side is a number
  (decimal text such as `99.6` is rounded) and as strings otherwise.
* Like other generic query checks, performance data is emitted once you choose
  a `perf-syntax` (there is no meaningful default alias for arbitrary
  queries); the thresholded columns then appear as perf values.
* Statements that produce no result set (a lone `UPDATE`, `SET`, ...) are
  reported as UNKNOWN rather than silently OK — the check is for reading
  state, not mutating it.
* Use a read-only monitoring account: the query runs with whatever privileges
  the configured user has.
