---
icon: "🔧"
modules: [core, docs]
action: none
---
**Modules declare a category, and the module list filters by it.** Nothing to
do. Every module's `module.json` now says what kind of module it is -
`check`, `database`, `container`, `server`, `client`, `script`,
`scheduling` or `tool` - and `/api/v2/modules` reports it as
`metadata.category`. The web UI's *Modules* page offers a row of groups built
from it (Checks, Databases, Containers & VMs, Servers, Clients, Scripts,
Scheduling & results, Other), with the number of modules in each; pick one or
more to list only those, and the choice is kept in the address. A database or
container check is listed under Checks too.

A zip bundle can declare a `"category"` in its `module.json` the same way; one
that does not is listed under *Other*. The reference's module navigation now
follows the categories as well, so *External scripts* moves to the helper
modules.
