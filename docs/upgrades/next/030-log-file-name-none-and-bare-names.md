---
icon: "🔧"
modules: [core]
action: conditional
---
**`file name = none` now really disables the log on Windows, and a bare log
file name gains its missing separator.** Nothing to do unless
`[/settings/log] file name` is set to `none` or to a name with no directory in
it; unix installations are unaffected either way.

The log file name was joined to the installation directory by string
concatenation, and the `none` sentinel was tested only *after* that join. On
Windows, where the installation directory is not empty, both went wrong:

| `file name` | Wrote to (before) | Writes to (now) |
|---|---|---|
| `none` | `C:\Program Files\NSClient++none` | nothing — file logging is off, as documented |
| `nsclient.log` | `C:\Program Files\NSClient++nsclient.log` | `C:\Program Files\NSClient++\nsclient.log` |

So a Windows host that had switched file logging off was still writing a log,
and one using a bare file name was writing it beside the installation
directory rather than inside it. If either applies, the old file is left where
it is — delete it once you have checked you do not need its contents.

An absolute path, or any name containing `/` or `\`, was never affected.
