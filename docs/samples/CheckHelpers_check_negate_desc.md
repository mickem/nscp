#### About `check_negate`

`check_negate` runs another check and **remaps its status**, leaving the message
and performance data untouched. It is the NSClient++ equivalent of the Nagios
`negate` plugin, and is available under the alias `negate`.

The command to run is named with `command=` (`-q`) and its arguments are passed
one per `arguments=` (`-a`). The four mapping options — `ok=` (`-o`),
`warning=` (`-w`), `critical=` (`-c`) and `unknown=` (`-u`) — each name the
state to return *instead of* that one. Every mapping defaults to itself, so the
options you omit pass through unchanged, and state names are parsed the usual
way (`ok`, `warning`, `critical`, `unknown`).

The classic use is inverting a check — "alert when this process *is* running",
"alert when this port *is* open" — by mapping `ok=critical critical=ok`. The
mappings are applied to the *original* status rather than chained, so that pair
is a clean swap and not a two-step collapse.

Unlike [`check_always_ok`](#check_always_ok) and its siblings, `check_negate`
keeps the distinctions between states; use it whenever you want to relabel
rather than flatten. If the wrapped command cannot be executed the check fails
outright and no mapping is applied.
