---
icon: "🧰"
modules: [packaging]
action: none
---
**The bundled `check_nsclient` is upgraded to 1.2.0.** The client that ships
with the Windows MSI and zip and with the Linux packages gains three commands:
`check_nsclient nsclient failures` reads the agent's failed-check log and the
metrics and diagnostics kept with each failure; `check_nsclient fleet explain`
asks an NSClient++ fleet server to explain an error; and `check_nsclient nrpe`
runs a check over NRPE, the way `check_nrpe` does, ending as UNKNOWN on every
failure. The facts text table also truncates long cells now. Nothing to do.
`nsclient failures` needs the `failures.list` grant on the agent.
