---
icon: "🔧"
modules: [packaging, core]
action: none
---
**The installer asks where the configuration comes from.** The first page of the Windows installer used to ask which
monitoring vendor was on the other end (*Generic* or *op5*). It now asks the question that decides what the rest of the
install does: **Select Management Server**.

| Answer             | `MANAGEMENT_SERVER` | What it means                                                              |
|--------------------|---------------------|----------------------------------------------------------------------------|
| None               | `NONE` (default)    | This machine keeps its own configuration, exactly as before.                 |
| NSClient Fleet     | `FLEET`             | Enroll with a [fleet server](../setup/fleet.md) while installing.            |
| Web server         | `WEB`               | An `nsclient.ini` served over HTTP(S) is the configuration.                  |

Nothing changes for an install that answers *None*, which is the default and what an upgrade of an unmanaged host
keeps doing. A managed answer skips the configuration and module pages, writes no local baseline over what the
management server sends, and puts a fresh install on the
[modern layout](../concepts/file-layout.md).

Silent installs need no change: `FLEET_SERVER`, or a `CONFIGURATION_TYPE` that is a http(s) url, already picks the
mode. Such a derived mode deliberately leaves the on-disk layout alone - pass `MANAGEMENT_SERVER=FLEET`/`WEB` (or
`LAYOUT=modern`) to ask for the move as well, and read
[On-disk layout](../setup/installing.md#on-disk-layout-layout) first, because it is one-way.
