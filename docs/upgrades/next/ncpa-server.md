---
icon: "🆕"
modules: [NCPAServer]
action: none
---
**New, experimental `NCPAServer` module: poll NSClient++ with Nagios'
`check_ncpa.py`.** Nothing changes for existing installs; the module is off
until enabled. It serves the Nagios NCPA HTTP API on port 5693, authenticated
with the NCPA token (`token`, and a `backup token` for rotation, under
`[/settings/NCPA/server]`) and restricted by `allowed hosts`. This release
serves the `plugins/` node, which runs any NSClient++ check, alias or external
script (`check_ncpa.py -M plugins/check_cpu`) and returns its output
unchanged; the built-in NCPA node tree (`cpu/`, `memory/`, `disk/`, ...)
follows. Arguments are refused unless `allow arguments = true`, as with NRPE.
The listener uses the WEB server's certificate and refuses to start without
one. On Windows it is the new *NCPA support* installer feature. See the
[NCPAServer reference](../reference/client/NCPAServer.md).
