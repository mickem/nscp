---
icon: "🔧"
modules: [CommandClient]
action: conditional
---
**`nscp test` now has a real prompt, and writes a command history file.** On a
terminal the prompt gained line editing, persistent history, tab completion
against the command registry, syntax highlighting that shows an unknown query
or module name in red before you press enter, and log messages that redraw
around what you are typing instead of landing in the middle of it. See
[Test mode](../concepts/test-mode.md). Two things to know:

- **History is written to a per-user file** —
  `%APPDATA%\NSClient++\console-history.txt` on Windows,
  `$XDG_STATE_HOME/nscp/console-history` or `~/.nscp_history` elsewhere
  (created mode `0600` on POSIX). Commands typed at the prompt can carry
  credentials, so if you would rather keep nothing on disk set
  `history size = 0` under `[/settings/cli]`. That section also holds
  `history file` and `color`.
- **With stdin not a terminal nothing changes** — no prompt, no history, no
  colour, and the core keeps writing the log itself. Piping commands in does
  now work on Windows, where the readiness check used a console-only API and
  silently ignored a file or pipe; an exhausted stdin no longer spins at 100%
  CPU on POSIX; and end of input is no longer treated as a reason to exit,
  which is how the agent is normally started under a supervisor.

Completion after `load`/`enable` offers the modules that are not already
loaded or enabled, and `unload`/`disable` the ones that are. The first such
completion in a session pauses while the agent reads the module directory,
once per process.
