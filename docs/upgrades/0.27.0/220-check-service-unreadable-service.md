---
icon: "🩹"
modules: [CheckSystem]
action: none
---
**`check_service` no longer fails on a service it cannot read.**
A service whose security descriptor refused `SERVICE_QUERY_CONFIG` to the
agent's account (common when NSClient++ does not run as LocalSystem) made
every `check_service` call fail with `Failed to open service: <name>`, and the
Checkmk `<<<services>>>` section disappeared with it. Such a service is now
listed with the state its status reports and a start type of `unknown`, and is
judged on that state alone: only a service that stopped on an error is flagged.
A service whose configuration query fails reports `unknown` too, where it used
to show `boot`.
