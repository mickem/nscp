---
icon: "🔒"
modules: [CheckDocker]
action: conditional
---
**`host=` on a docker check must now match the configured endpoint.** Which
daemon the agent talks to is decided by `endpoint` under `[/settings/docker]`;
a request repeating that value is accepted, one naming a different socket or
pipe is refused. The agent would otherwise connect to any local socket or
`\\.\pipe\<name>` a caller named and report back what happened, which is a
read-only probe of the host as `SYSTEM` or `root`. Check definitions that spell
out `host=` keep working as long as they name the configured endpoint; otherwise
drop the argument or change the setting.
See the [security notice](../security/notices.md#script-execution-and-check-arguments-nul-truncation-import-sandbox-pipe-reads-handle-leak-docker-endpoint-remote-connection-checks).
