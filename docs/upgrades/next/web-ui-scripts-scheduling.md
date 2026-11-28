---
icon: "🔧"
modules: [CheckHelpers, CheckExternalScripts, Scheduler, LUAScript, PythonScript, DotnetPlugins]
action: none
---
**Web UI: pages for aliases, scripts and schedules.** Nothing to do.

- **Aliases** (Check helpers, and External scripts' own): each alias with the
  check it runs. The dialog picks the check from those the agent has loaded,
  offers its options as chips, starts from the presets, runs the line at once
  when it takes no `$ARGn$`, and says what the agent's alias parser refuses -
  only `\\`, `\"` and `\<space>` are escapes, single quotes are plain
  characters, and a line break is refused.
- **External scripts**: whether callers may pass arguments, and shell
  characters, with the risk said plainly and a fixed alias in Check helpers
  offered instead; the kill timeout; the script folder under *Advanced*.
  Scripts are one-line entries, or sections once they run as another user or
  change how their output is read. A wrapped script is a file and its
  arguments, with the command line its extension's wrapping makes of them
  shown beside it - and a warning when no wrapping matches the extension
  (the match is case sensitive). The built-in ps1, vbs and bat wrappings are
  listed with the ones you add.
- **Scheduler**: the worker count, startup spread and timezone, and an editor
  for schedules - the check (and, behind *Rename*, the service name it is
  submitted as), an interval with a *Fuzzy* switch that spreads the runs by
  20% or a cron line, the channel the result is submitted on (picked from
  the usual ones, or typed), the target, and which results to submit. `default` is
  shown as what every schedule takes, since every schedule key inherits. A
  schedule the agent would refuse is marked: one with both an interval and a
  cron line (one of them possibly inherited), with neither, or with a cron
  field that is not `*` or a list of numbers.
- **Lua, Python and .NET**: their folders, and the scripts or plugins they
  load.

On every module page with its own settings panel, the panel's unsaved
changes, *Save* and *Discard* are a bar always pinned to the bottom of the
window, so they stay in reach below the lists
(which save each change from their own dialog). *Show config* sits in the
same bar and prints the lists' sections too; a page that is only lists (the
clients, Check helpers) gets a bar with *Show config* alone.
