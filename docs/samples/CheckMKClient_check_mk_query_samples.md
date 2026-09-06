**Query a remote check_mk agent:**

A stock check_mk agent listens on TCP 6556, so the port normally has to be given
explicitly — the module's own default is 5667.

```
check_mk_query host=192.168.56.20 port=6556
OK: check_mk agent responded
```

**Use a configured target:**

```ini
[/settings/check_mk/client/targets/linux01]
address = 192.168.56.20:6556
timeout = 30
```

```
check_mk_query target=linux01
OK: check_mk agent responded
```

**What the check actually reports is decided by the Lua script:**

The agent returns a sectioned plain-text dump (`<<<mem>>>`, `<<<df>>>`,
`<<<ps>>>`, ...), and a Lua script registered on the module is called back with
the parsed packet to produce the status and message. With no script configured,
`default_check_mk.lua` is loaded.

```ini
[/settings/check_mk/client/scripts]
mine = check_mk_custom.lua
```

```
check_mk_query target=linux01
CRITICAL: /var 94% used
```

Change the script, not the command line, when you want different behaviour.

**Nothing listening:**

```
check_mk_query host=127.0.0.1 port=15670
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15670 :Connection refused
```

**A note on the transport:**

A stock check_mk agent speaks plain text on 6556 with access control by
source-IP allowlist rather than authentication. Treat it as unauthenticated
unless you have put TLS in front of it, and configure `ca` / `verify` /
`certificate` accordingly where you have.

See also the CheckMKServer module for the passive direction — serving check_mk
agent output *from* this host.
