---
icon: "🏷️"
modules: [core]
action: conditional
---
**`${hostname}` in an existing config changes meaning** — it is now expanded
everywhere `expand_hostname` is used (including submit clients' `hostname`),
where it used to be left as literal text.
