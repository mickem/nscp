---
icon: "🔒 ⏱️"
modules: [LUAScript, CheckMKServer, CheckMKClient]
action: none
---
**Lua checks now each run on their own interpreter stack, so concurrent checks
against one script no longer crash the agent.** Nothing to do on a default
install, and nothing to change in existing scripts.

Two checks against the same Lua script used to share one execution stack
whenever the script called back into the core or slept, which corrupted the
interpreter and killed the process. Each invocation now gets its own coroutine
off the script's state; globals, `require`d modules and anything else the script
keeps at file scope are still shared between invocations exactly as before. See
the [security notice](../security/notices.md#luascript-concurrent-checks-in-one-script-crashed-the-agent).

Two related changes are visible only in timing: a script that calls into the
core no longer keeps the interpreter locked against other Lua checks while it
waits (previously a nested call left it locked, so those checks took turns), and
loading, starting and unloading a script now takes the interpreter lock, so a
settings reload waits for a Lua check that is mid-run instead of re-entering the
script underneath it.
