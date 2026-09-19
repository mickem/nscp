---
icon: "🔒 💥"
modules: [PythonScript, LUAScript]
action: conditional
---
**Python and Lua scripts must now live inside the script folder, or a folder you
name.** Nothing to do if your scripts are under `${scripts}`, which is the
normal case. **If any `[/settings/python/scripts]` or `[/settings/lua/scripts]`
entry points outside it — an absolute path to a vendor plugin, or a path
containing `..` — that script will no longer load until you add its folder.**

A configured script is now checked against a list of allowed roots before it is
loaded. The list starts with `${scripts}` and you extend it:

```ini
[/settings/python]
additional script roots = /usr/lib/nagios/plugins, ${shared-path}/vendor

[/settings/lua]
additional script roots = /opt/acme/libexec
```

Entries are comma separated and each one is expanded, so path tokens work.

The refusal is logged with both the script and the folders that were allowed, so
an upgrade that breaks a script tells you exactly what to add:

```
Refusing to load script outside the allowed roots: /opt/acme/libexec/check.py
(allowed: /usr/lib/nsclient/scripts). Add its folder to 'additional script
roots' under the python section if it belongs there.
```

Why: the script search resolves a configured name by joining it onto the script
folder, and that join never stopped the value climbing back out — `../foo.py`
resolved to `${scripts}/../foo.py` and loaded from the installation directory.
The `nscp py` / `nscp lua` CLI has always refused to read or delete a script
outside the script root; the path that actually *runs* code had no such check,
so what the CLI refused, the configuration could still execute. See the
[security notice](../security/notices.md#configured-scripts-could-be-loaded-from-outside-the-script-folder).

`[/settings/external scripts]` is unaffected: it takes a command line rather
than a path the agent resolves, and running an arbitrary command is what it is
for.
