---
title: "PythonScript: reading and deleting scripts over REST and the CLI"
fixed_in: next
severity: "Low"
modules: [PythonScript, WEBServer]
action: conditional
---
`GET` and `DELETE` on `/api/v2/scripts/py/<name>` - and the `nscp py show` and
`nscp py delete` verbs behind them - used to do nothing: they answered an empty
`200` and changed nothing. They now read and delete script files, which is what
the [scripts API](../api/rest/scripts.md) documents and what the `ext` runtime
already did. That is new reach for anyone holding the grants:

| Grant                         | Now allows                                                                    |
|-------------------------------|-------------------------------------------------------------------------------|
| `scripts.get.PythonScript`    | Reading any file under `${scripts}/python`                                    |
| `scripts.delete.PythonScript` | Deleting any file under `${scripts}/python`, and the configuration entries that load it |

The built-in `full` role (`*`) holds both; no other built-in role does.

Both are confined to `${scripts}/python`. A name is resolved inside that folder
only - never relative to the working directory, never as an absolute path - and
the check is made on real paths with symlinks resolved, so a symlinked
sub-folder cannot reach outside it. A path that cannot be resolved is refused.
`show` reads a symlink only when its target is inside the folder as well.
`delete` removes the entry itself - a file, or a symlink whatever it points at -
and never follows a link, so it cannot remove anything outside the folder. Over REST the name is also rejected before any of this if
it holds `..`, a leading `/` or a drive letter.

**What to do:** nothing on a default install. If a custom role holds
`scripts.delete.PythonScript`, `scripts.get.PythonScript` or `scripts.*` and was
not meant to read or remove Python scripts, take the grant away.
