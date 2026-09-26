---
title: "External scripts: arguments to a .bat or .cmd are held to the shell rules"
fixed_in: next
severity: "High where an operator configured a batch script with argument pass-through, none otherwise"
modules: [CheckExternalScripts]
action: conditional
---
`CheckExternalScripts` builds an argv vector whenever it can tokenise the
command template, and hands it to `CreateProcess` with `lpApplicationName` set,
so the launcher never re-tokenises the command line and a caller's `$ARG1$`
cannot become extra argv elements. On that path the looser `NASTY_METACHARS`
set is applied, which deliberately permits `%`, `^`, `!`, `(`, `)`, `$` and
newlines so that check syntax such as `top-syntax=%(status): %(list)` keeps
working over NRPE.

That reasoning does not hold for a batch file. `CreateProcess` cannot execute a
`.bat` or `.cmd`: given one it re-launches `cmd.exe /c <command line>`, and
cmd.exe then parses that line by its own rules. `lpApplicationName` pins which
program runs, not how its arguments are read. So on that path `%VAR%` still
expanded from the service environment, `^` still escaped, and a CR or LF inside
an argument ended the statement — everything after it parsed as a fresh
command, with no quote to break out of. This is the mechanism behind the
BatBadBut class of vulnerabilities (CVE-2024-24576 and relatives).

The argv path was reached whenever the template tokenised, which a
backslash-free template does: a forward-slash path or a UNC path to a `.bat`.
For an operator who had configured such a command, any caller allowed to pass
arguments could run commands as the service account, normally SYSTEM.

Two changes, both independent of each other:

* `CheckExternalScripts` now recognises a `.bat` or `.cmd` target and validates
  caller-supplied argument values against the stricter `SHELL_METACHARS` set,
  the same one the single-string fallback uses. The refusal names the reason.
  `allow nasty characters = true` still overrides it, as before.
* The Windows launcher refuses outright to run a batch target when any argument
  contains a carriage return or a line feed, whatever the caller validated.
  There is no legitimate newline inside an argument to a batch file, and this
  one is not about what the argument means to the script but about whether it
  stays one argument at all.

**What to do:** nothing on a default install — no shipped command template
invokes a batch file. If you configured one and pass arguments to it, check
whether your arguments contain any of `$ ; ( ) * ? ~ ! % ^` or a newline; they
will now be refused. Prefer rewriting the script as an `.exe`, or as a `.ps1`
invoked through an explicit `powershell.exe` template, over setting
`allow nasty characters = true`.
