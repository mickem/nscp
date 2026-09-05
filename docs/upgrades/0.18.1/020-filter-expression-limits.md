---
icon: "🔒"
modules: [filters]
action: none
---
**Filter expressions are now bounded in length and nesting depth.** A
`filter` / `warning` / `critical` expression — and a `%(...)` expression
placeholder inside a syntax template — longer than **1024 characters** or
nested more than **64** parentheses deep is now rejected at parse time
instead of being evaluated, failing with a clear "exceeds the maximum
length/depth" error. The where-parser and the expression evaluator both
recurse with the shape of the input, so an unbounded or deeply nested
expression could exhaust the stack and crash the agent. Real filters are a
small fraction of these limits, so the default install and every normal
configuration are unaffected — only a pathologically large or deeply nested
expression is refused. See the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
