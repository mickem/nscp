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
"alert when this port *is* open".

**Beware that the mappings are applied in sequence to the value as it is being
rewritten, not to the original status.** The order is OK, WARNING, CRITICAL,
UNKNOWN, so a mapping that moves a status *forward* in that order can be picked
up and rewritten again by a later rule. `ok=critical critical=ok` is therefore
**not** a clean swap: an OK result is rewritten to CRITICAL by the first rule and
then straight back to OK by the third, so the OK half of the inversion silently
does nothing. The CRITICAL half works, because nothing after it rewrites OK.

The mappings that are safe are the ones that move a status *backwards*
(`critical=warning`, `unknown=critical`, `warning=ok`) and any single mapping
whose destination you do not also remap. To invert a check reliably, map only
the direction you actually need:

```
check_negate command=check_thing critical=ok
```

Unlike [`check_always_ok`](#check_always_ok) and its siblings, `check_negate`
keeps the distinctions between states; use it whenever you want to relabel
rather than flatten. If the wrapped command cannot be executed the check fails
outright and no mapping is applied.
