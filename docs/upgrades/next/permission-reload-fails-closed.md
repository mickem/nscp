---
icon: "🔒"
modules: [core]
action: conditional
---
**A permission policy reload that fails part-way now denies exec too, and a failure to register the settings keys no longer counts.**
Nothing to do unless `[/settings/permissions] enabled = true`. A reload that
threw part-way used to leave whatever it had got to: usually an emptied rule
table, with `enabled` and `allow exec` kept as they were unless already re-read,
or the whole previous table when the failure came before the reload started
(registering the settings keys). Now the rules read before the failure are
enforced, every other call is denied, and exec is denied unless `allow exec`
was read, until a later reload loads the policy completely. A failure before
`enabled` is read leaves the policy on or off as it was, so a host that never
enabled it is unaffected; at boot that means the policy stays off until a
reload reads it. Registering the settings keys (documentation metadata) can
no longer fail the load. Look for `permissions: failed to load` in the log for
the cause. See the
[security notice](../security/notices.md#settings-reload-races-included-settings-stores-and-the-permission-table).
