---
icon: "🔧"
modules: [CheckHelpers, CheckExternalScripts]
action: conditional
---
**An alias containing `\n` is now refused instead of getting a newline.**
Alias definitions treat `\` as an escape character, and only
`\\` and `\"` are escapes. Every other backslash already made NSClient++ refuse
the alias, except `\n`, which was turned into a real newline. So
`"path=C:\nscp"` silently became `C:` + newline + `scp`. Those aliases are now
refused too, and the log line (`Refusing alias '…' - command line did not parse
cleanly`) names the position and the fix. If an alias uses `\n` on purpose, for
example `"list-separator=\n"`, write it as `"list-separator=\\n"`; write Windows
paths with `/` or doubled backslashes. After upgrading, check the log for
refused aliases.
