---
icon: "🔧"
modules: [CheckExternalScripts]
action: conditional
---
**Windows: a relative external-script command is resolved against the installation directory.**
`[/settings/external scripts/scripts]` entries that carry a folder — the conventional
`check_foo = scripts\check_foo.bat`, and what `nscp ext-scr add --import` writes — are now rooted
at `${base-path}` before they are launched. On Windows that is the same folder `${scripts}` points
at, so the script is found wherever the agent was started from.

Previously such a command was measured against the **working directory of the process**, which is
`C:\Windows\System32` for the service and the shell's directory for `nscp test`. The effect was
easy to miss because it depended on how the command happened to be spelled: a single backslash
(`scripts\check_foo.bat`) is not tokenisable as an argument vector, so it fell back to the legacy
single-string launcher, where the lookup already honoured the installation directory and the
command worked. A command that *was* argv-safe — a doubled backslash, a forward slash, a quoted
path — ran with argv-isolation, where the executable is named separately and was resolved against
the working directory, and it failed for the service with "the system cannot find the path
specified".

Check your configuration if either applies:

* **You worked around this with an absolute path.** Nothing to do — an absolute path,
  a UNC path, a drive-relative `C:check.exe` and a root-relative `\tools\check.exe` are all still
  used exactly as written.
* **You relied on the command resolving against the working directory** — a relative command
  pointing at a folder outside the installation directory, with the agent started from somewhere
  that made it resolve. That no longer works; name the script with an absolute path, or with
  `${scripts}\<name>` if it lives in the script folder.

A command with no folder at all (`cmd.exe`, `powershell.exe`, `cscript.exe` — what the shipped
`[/settings/external scripts/wrappings]` use) is **not** rooted; rooting it at the installation
directory would point every one of them at a file that is not there. It now gets the system's own
executable search instead — the directory the agent loaded from, the working directory, the system
and Windows directories, then `PATH`. On the argv-isolated path it previously got no search at all
and resolved against the working directory alone, so `command = cmd.exe /c …` only ran when the
agent happened to be started from a directory containing a copy of `cmd.exe`. The shipped wrappings
were not affected, because their backslash paths put them on the legacy launcher, which has always
done the search.

Linux is unchanged: the unix launcher sets no working directory for the child and does not root the
command, so a relative command still resolves against the agent's working directory — which the
shipped systemd unit sets to `${shared-path}`.
