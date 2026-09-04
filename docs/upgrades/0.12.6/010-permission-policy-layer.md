---
icon: "🔒"
modules: [core]
---
**New permission policy layer, disabled by default.** Existing installs
behave exactly as before until an operator sets
`/settings/permissions/enabled = true`. If you opt in: per-command rules apply
to **queries only** (exec is gated by the separate `allow exec` boolean, which
defaults `true`); roll out with `log allows = true` first to inventory real
traffic. See [Permissions](../concepts/permissions.md).
