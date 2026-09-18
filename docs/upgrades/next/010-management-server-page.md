---
icon: "🔧"
modules: [packaging, core]
action: conditional
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
keeps doing.

**Check this if you install with `FLEET_SERVER=` or a http(s) `CONFIGURATION_TYPE=`.** Such a command line has always
described a managed agent, and the installer now treats it as one: it writes no local baseline. Previously it also
wrote the generic starting point — `allowed hosts = 127.0.0.1`, a generated password, NRPE in secure mode and the six
common check modules — into `nsclient.ini` before adding the fleet include or the url.

That baseline is why the change is worth making: a value in `nsclient.ini`
[wins over the one the management server sends](../setup/fleet.md#step-5-send-it-some-configuration), so the module
list written at install time quietly shadowed the one being maintained centrally. Agents that looked managed were
running their install-time configuration.

What to do, if anything:

- **Managing those settings from the fleet server or the served `nsclient.ini` is the point** — send them from there
  and you need nothing here. This is what the change makes possible.
- **To keep a local baseline anyway**, name the values on the command line; explicit properties are still written in
  every mode. For the old defaults that is `ALLOWED_HOSTS=127.0.0.1 CONF_CHECKS=1 CONF_NRPE=1 CONF_WEB=1` plus an
  `NSCLIENT_PWD=` of your own. Keep it small: everything set here still shadows what the server sends.
- **The on-disk layout is untouched** for these command lines. A mode worked out from `FLEET_SERVER` or
  `CONFIGURATION_TYPE` deliberately does not get the modern-layout default that naming `MANAGEMENT_SERVER` or picking
  the answer on the page does, because that migration is one-way. Pass `LAYOUT=modern` to ask for it, and read
  [On-disk layout](../setup/installing.md#on-disk-layout-layout) first.
