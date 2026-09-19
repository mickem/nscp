---
icon: "🔧"
modules: [LUAScript, PythonScript]
action: conditional
---
**`nscp lua` and `nscp py` now look in, and import into, the folder scripts are
actually loaded from.** Nothing to do unless you use those subcommands to
manage scripts; loading scripts configured in `nsclient.ini` is unchanged, and
existing configuration entries keep working.

Both ext-scr CLIs derived their folder by appending a literal `scripts` segment
to a root, rather than naming `${scripts}` — the token that already means that
folder. The two are the same thing only on Windows, where `${scripts}` is
`${exe-path}/scripts`:

| | Looked in / imported into (before) | Now |
|---|---|---|
| `nscp lua list/show/add/delete` | `${base-path}/scripts/lua` — right on Windows, `/usr/sbin/scripts/lua` on Linux | `${scripts}/lua` |
| `nscp py list/show/add/delete` | `${scripts}/scripts/python` — a folder nothing creates | `${scripts}/python` |

So on Linux the Lua subcommands were looking beside the binary rather than in
the package's script folder, and the Python ones imported into a doubled path
on every platform. `nscp` for external scripts (`CheckExternalScripts`) was
already correct and is unchanged.

`add --import` also records a different value in the configuration. It used to
write `scripts\python\<name>` / `scripts\lua\<name>`, which paired with the old
folders and only resolved on Windows, where a backslash is a separator. New
imports record `python/<name>` / `lua/<name>`, relative to `${scripts}` and
resolving the same way on both platforms.

Entries written by an older version are not rewritten and keep resolving as
before — the file they point at has not moved.
