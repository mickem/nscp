---
icon: "🔧"
modules: [core, SimpleFileWriter]
action: conditional
---
**A relative path in a setting that writes files is now taken relative to that
setting's own folder, not to the service's working directory.** Nothing to do
if your paths are absolute or written with `${...}` tokens. If any of the
settings below is a bare relative name, the file it produces moves — on unix
usually not at all, on Windows usually into the folder it should always have
been in.

Expanding a path substitutes `${tokens}`; it never made anything absolute. A
value with neither a token nor a leading `/` was therefore resolved against
whatever the service's working directory happened to be, which is
`C:\Windows\System32` for a Windows service, `/` under a bare init script, the
package directory under the shipped systemd unit, and the shell's directory for
`nscp test`. Four different answers from the same configuration file.

Settings whose consumers own a folder now say so, and a relative value lands
there instead:

| Setting | Relative values now land in |
|---|---|
| `[/attachments]` target | `${shared-path}` |
| `[/settings/log] file name` | `${log-path}` |
| `[/settings/crash] archive folder` | `${crash-folder}` |
| `[/settings/fleet] managed path` | `${fleet-folder}` |
| `[/settings/filewriter] file` | `${log-path}` |

An absolute path, a UNC path or anything written with a token is used exactly
as given — pointing one of these somewhere specific is still yours to decide.
`none` still means "no file" wherever it was already accepted.

On Linux the attachment case generally does not move: the shipped systemd unit
sets `WorkingDirectory` to the package directory, which is what `${shared-path}`
resolves to, so relative attachments were already landing in the right place —
by coincidence rather than by design. Windows services and any non-systemd
launch are where this changes something.

`SimpleFileWriter`'s default `file = output.txt` is affected by the same rule
and now writes to `${log-path}/output.txt` rather than to the working directory.
