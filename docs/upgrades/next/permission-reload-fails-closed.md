---
icon: "🔒"
modules: [core]
action: conditional
---
**A permission policy reload that fails part-way now denies instead of keeping the old rules.**
Nothing to do unless `[/settings/permissions] enabled = true`. Before, a
reload that threw part-way kept the whole previous rule table in force,
including rules the operator had just removed and the previous `allow exec`
setting. Now the rules read before the failure are enforced, every other call
is denied, and exec is denied unless `allow exec` was read, until a later
reload loads the policy completely. A failure before `enabled` is read leaves
the policy as it was, so a host that never enabled it is unaffected; at boot
that means the policy stays off until a reload reads it. Look for
`permissions: failed to load` in the log for the cause. See the
[security notice](../security/notices.md#settings-reload-races-included-settings-stores-and-the-permission-table).
