---
title: "Configured scripts could be loaded from outside the script folder"
fixed_in: next
severity: "Low"
modules: [PythonScript, LUAScript]
action: conditional
---
`PythonScript` and `LUAScript` resolve a configured script name by trying a list
of candidates, the last of which joins the value onto the script folder. That
join never stopped the value climbing back out of it: a
`[/settings/python/scripts]` entry of `../foo.py` resolved to
`${scripts}/../foo.py` and was loaded — and executed — from the installation
directory. An absolute path anywhere on the filesystem was accepted too, since
the search tries the value as-is first.

The `nscp py` and `nscp lua` CLIs have always refused to read, import or delete
a script outside the script root, for exactly this reason. The path that
actually runs code had no equivalent check, so what the CLI refused to show you,
the configuration could still execute.

This is a hardening change rather than a privilege boundary being crossed:
writing to `nsclient.ini` already implies control of the agent, and no
unauthenticated or remote input reaches these values. It matters where the
configuration is not wholly trusted — a fleet-managed or templated
`nsclient.ini`, or a setup where operators may edit the script sections but are
not meant to reach outside the script folder — and because a refusal the CLI
makes should not be one the loader ignores.

A configured script is now checked against a list of allowed roots, starting
with `${scripts}`, and a script outside them is refused with a message naming
both the script and the roots.

**What to do:** nothing if your scripts live under `${scripts}`. If you run
scripts the agent does not own — a plugin package's own `libexec`, a vendor
directory — name those folders so they keep loading:

```ini
[/settings/python]
additional script roots = /usr/lib/nagios/plugins, ${shared-path}/vendor
```

Entries are comma separated and path tokens are expanded. Keep the list to the
folders you actually use: adding a world-writable directory would give back the
exposure this closes.

`[/settings/external scripts]` is not affected and not sandboxed — it takes a
command line rather than a path the agent resolves, and running an arbitrary
command is its purpose.
