---
icon: "🔒"
modules: [LUAScript, CheckMKServer, CheckMKClient]
action: none
---
**LUAScript: a Lua check that runs twice at once no longer crashes the agent.**
Nothing to do. Two concurrent calls into the same Lua script, where the first
was parked in `nscp.sleep` or a `Core` call, shared one Lua stack and could
terminate `nscp`; that needed only two overlapping NRPE requests for such a
check, or a scheduler running it on two threads. Every call now runs on a Lua
thread of its own. See the
[security notice](../security/notices.md#luascript-concurrent-calls-into-one-script-crashed-the-agent).
