---
title: "WEB: the monitoring role reads the facts inventory"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: conditional
---
The bundled `monitoring` role now carries `facts.get`, so on a **fresh
install** a `monitoring` user can read `GET /api/v2/facts`: whatever fact sets
the operator enabled (OS and hardware, volumes, network interfaces, installed
software, services, scheduled tasks, containers, databases). A monitoring
server builds its checks from exactly that list, which is why the role has it.
It does not carry `facts.refresh`, so a `monitoring` user still cannot make the
producers collect.

Nothing is in the inventory unless a fact set was enabled, and existing
configurations are not rewritten: a `monitoring` role string already in
`nsclient.ini` is left as it is, so nothing widens under an upgrade. The
`client`, `metrics` and `restricted` roles do not carry `facts.get`.

**What to do:** if you assign the built-in `monitoring` role on a new install
and do not want its users to read the inventory, pin the grants you want in
`[/settings/WEB/server/roles]`:

```ini
[/settings/WEB/server/roles]
monitoring = public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list
```

A scraper that only needs metrics belongs on the `metrics` role, which has
neither checks nor facts.
