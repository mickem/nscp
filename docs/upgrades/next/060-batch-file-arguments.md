---
icon: "🔒"
modules: [CheckExternalScripts]
action: conditional
---
**Arguments to a `.bat` or `.cmd` external script are held to the shell
rules.** Nothing to do on a default install; no shipped command template
invokes a batch file. Windows cannot execute a batch file directly — it
re-launches `cmd.exe /c` with the command line — so the argv isolation that
makes the looser metacharacter set safe for a real executable does not apply:
`%VAR%` still expanded, `^` still escaped, and a newline ended the statement so
the rest ran as a separate command. A batch target now validates caller-supplied
argument values against the same stricter set the shell fallback uses, and the
launcher refuses a batch target outright when an argument contains a carriage
return or a line feed. If you configured a batch command and pass arguments
containing any of `$ ; ( ) * ? ~ ! % ^`, they will now be refused; prefer
rewriting the script over setting `allow nasty characters = true`. See the
[security notice](../security/notices.md#external-scripts-arguments-to-a-bat-or-cmd-are-held-to-the-shell-rules).
