#### About `check_always_critical`

`check_always_critical` runs another check and then **overwrites its status with
CRITICAL**, keeping the wrapped check's message and performance data intact.

The wrapped command and its arguments are passed positionally: everything after
the command name is handed to the wrapped check unchanged.

Use it to demote a check that is informational rather than actionable — you
still want its numbers graphed and its message in the service detail, but you do
not want it to page anyone. It is the blunt counterpart to
[`check_negate`](#check_negate), which remaps individual states rather than
collapsing all of them to one.

**The override is unconditional.** A misspelt command name, a module that is not
loaded, or a check that failed outright all come back as CRITICAL with the error
text as the message — the failure is visible only if somebody reads it. That
makes this a poor choice for anything you rely on to still be running; prefer
`check_negate` when you want to relabel some states but keep the ability to
notice that the check itself broke.

The legacy alias `CheckAlwaysCRITICAL` is accepted for backwards compatibility.
