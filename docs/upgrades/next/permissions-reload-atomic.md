---
icon: "🔒"
modules: [core]
action: conditional
---
**Permission policy reloads no longer deny checks while the table is rebuilt.**
Nothing to do, unless you rely on a reload to tighten the policy - then check
the log for `permissions: failed to load` after it. A settings reload used to clear the live permission table and
refill it rule by rule, so checks arriving in between were denied with
`permissions: denied` and returned UNKNOWN. The new policy is now built aside
and swapped in whole. A reload that fails to read its settings now keeps the
previous policy in force and logs `permissions: failed to load`, instead of
leaving a cleared or partial table behind. See the
[security notice](../security/notices.md#permission-policy-reloads-are-applied-in-one-step-and-keep-the-previous-policy-on-failure).
