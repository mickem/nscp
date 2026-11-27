---
icon: "🔧"
modules: [NSCAServer]
action: none
---
**`NSCAServer` now loads without a key; only its listener is refused.** With
encryption enabled and no (or a hashed) `password` under
`/settings/NSCA/server`, the module used to fail to load, which took it out of
the running agent - its settings could then not be listed or fixed from the
web UI or REST. It now loads, logs the same
`Refusing to start NSCA server: …` error and simply opens no port. Set the key
and reload the module to bring the listener up. Nothing is accepted without a
key, as before.
