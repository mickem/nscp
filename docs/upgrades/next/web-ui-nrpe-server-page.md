---
icon: "🔧"
modules: [WEBServer, NRPEServer]
action: none
---
**Web UI: a settings page for the NRPE server.** Nothing to do. The NRPEServer
module page replaces its setup wizard with a page that reads the stored
settings back into one of three security modes (two-way TLS, one-way TLS,
insecure) and one of three argument policies, says plainly when the result is
insecure, and lists the exact changes before saving them. A configuration that
matches none of the modes is shown as *Custom* and left alone, with a link to
expert mode; the old wizard rounded it to the nearest mode instead. The page
covers every NRPE setting (the rarely changed ones under *Advanced*), so the
generic settings list no longer appears below it; expert mode still shows every
key as the schema describes it. A module that is not loaded shows no settings
in either view.

Settings NRPE can inherit from `[/settings/default]` (`allowed hosts`,
`timeout`, `bind to`, `thread pool`, ...) carry a *Default* / *Local* switch:
*Default* keeps the value in the shared section, used by every server module
that does not set its own; *Local* keeps it for NRPE only. A setting shows
*Local* when NRPE already stores its own value, *Default* otherwise.

*Show config* prints the NRPE configuration as an INI fragment, including the
`[/modules]` line and, optionally, every value left at its default, ready to
paste into another agent's `nsclient.ini` or a fleet bundle.

Changes made on the page also differ from the old wizard:

- `use ssl` is written under its real name; the wizard wrote an `ssl` key the
  agent does not read, which can be removed.
- The arguments choice can also set CheckExternalScripts' `allow arguments`,
  which decides whether the arguments reach your scripts.
