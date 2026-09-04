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

If the wrapped command cannot be executed at all the result is UNKNOWN, not
CRITICAL: a broken configuration is never silently reported as a fixed status.

The legacy alias `CheckAlwaysCRITICAL` is accepted for backwards compatibility.
