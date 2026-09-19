---
icon: "📋"
modules: [core, CheckSystem, CheckDisk, WEBServer]
action: none
---
**The agent can now collect host inventory ("facts"), and collects none of it
unless you ask.** Nothing to do on an upgrade: a host that says nothing about
this in its configuration behaves exactly as before.

Facts are structured inventory about the machine — which volumes exist, what
the hardware is, which interfaces it has — as opposed to tags, which are the
flat `key=value` strings a fleet group selector matches on. Tags are unchanged.

Collection is opt-in per **fact set**, in a new `[/settings/facts]` section:

```ini
[/settings/facts]
os = true
storage.volumes = true
```

`nscp test` → `facts list` shows the sets this agent can produce, what each one
holds and what it costs to collect. Once a set is enabled, the document is on
`GET /api/v2/facts`, on the web UI's new **Inventory** page and in the console
(`facts`, `facts refresh`).

Two things worth knowing if you manage the configuration centrally:

* The built-in `monitoring` web role gained `facts.get`, so a monitoring server
  using that role can read the inventory. It did **not** gain `facts.refresh`,
  which forces an immediate collection and stays with `full`. A role you wrote
  yourself is untouched.
* A fleet bundle enables inventory for a group with the fragment above and no
  new protocol; the change takes effect on the next settings reload rather than
  needing a restart.
* Every state report to a fleet server now carries a `facts_hash` — the digest
  of the host's inventory, which on a host with nothing enabled is the digest
  of the empty document. The report still carries no inventory and no
  configuration. A server that does not understand the key ignores it, and a
  server that does not have the `/agent/v1/facts` route is detected once and
  not asked again.

See [Tags and Facts](../concepts/facts.md) for the document rules and the
privacy stance, and the [facts API](../api/rest/facts.md) for the endpoints.
