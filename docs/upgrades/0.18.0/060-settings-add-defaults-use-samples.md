---
icon: "🔧"
modules: [core]
action: conditional
---
**`settings --update --add-defaults --use-samples` now writes the sample
objects.** The flag was parsed and never read, so it behaved exactly like the
plain invocation. It now writes the registered `/sample` sections, and
`--remove-defaults` strips them again — the two are now exact inverses. An
edited sample is still never overwritten or removed, and without the flag the
output is byte-for-byte what it was. If you have scripted
`--add-defaults --use-samples` expecting today's (sample-free) output, drop
the flag.
