---
icon: "🔧"
modules: [core]
action: conditional
---
**A fleet server's host override now wins over the host's bundles.** The
desired state carries a host-specific override alongside the bundle list. The
agent used to merge the override first and every bundle's `config.json` on
top of it, so a key that both set took the bundle's value and the override
only applied to keys no bundle touched. The agent now merges the override
last, and says so in every state report (`host_override_last`), so the fleet
server can tell which hosts apply it.
If you have a host override that sets a key a bundle also sets, the host
will switch to the override's value when it updates. Check your host
overrides before upgrading if you relied on a bundle winning.
