---
title: "LUAScript: reading and deleting scripts over REST and the CLI"
fixed_in: next
severity: "Low"
modules: [LUAScript, WEBServer]
action: conditional
---
`GET` and `DELETE` on `/api/v2/scripts/lua/<name>` - and the `nscp lua show` and
`nscp lua delete` verbs behind them - used to do nothing: they answered an empty
`200` and changed nothing. They now read and delete script files, which is what
the [scripts API](../api/rest/scripts.md) documents and what the `py` and `ext`
runtimes already do. That is new reach for anyone holding the grants:

| Grant                      | Now allows                                                                    |
|----------------------------|-------------------------------------------------------------------------------|
| `scripts.get.LUAScript`    | Reading any file under `${scripts}/lua`                                       |
| `scripts.delete.LUAScript` | Deleting any file under `${scripts}/lua`, and the configuration entries that load it |

The built-in `full` role (`*`) holds both; no other built-in role does.

Both are confined to `${scripts}/lua`, with the rules PythonScript applies to its
folder. A name is resolved inside that folder only - never relative to the
working directory, never as an absolute path - and the check is made on real
paths with symlinks resolved, so a symlinked sub-folder cannot reach outside it.
A path that cannot be resolved is refused. `show` reads a symlink only when its
target is inside the folder as well. `delete` removes the entry itself - a file,
or a symlink whatever it points at - and never follows a link, so it cannot
remove anything outside the folder. Over REST the name is also rejected before
any of this if it holds `..`, a leading `/` or a drive letter.

**What to do:** nothing on a default install. If a custom role holds
`scripts.delete.LUAScript`, `scripts.get.LUAScript` or `scripts.*` and was not
meant to read or remove Lua scripts, take the grant away.
