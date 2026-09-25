---
icon: "📤"
modules: [core]
action: conditional
---
**An enrolled agent now sends its host facts to the fleet server.** Nothing to
do on a default install: no fact set is enabled by default, and a host with
none enabled sends only the hash of the empty document. If you have enabled
fact sets on a host that is enrolled with a fleet server, that inventory now
leaves the host.

* Every state report carries `facts_hash`, the SHA-256 of the facts document,
  never the document itself.
* The document is uploaded on its own call, `POST /agent/v1/facts`, only when
  it differs from what the server holds: after a round changed it, once after
  the agent starts, and when the server says it holds something else.
* A fleet server without the call answers 404, and the agent stops offering
  the document until the server starts asking for it. Nothing is logged at
  error level for that.
* An upload is capped by `[/settings/facts] max size`. A document over it, or
  one the server refuses as too large, is not sent again until it changes, and
  the log names the largest sets.

To keep a set on the host but off the server, there is no separate switch:
turn the set off, or do not enroll the host. See
[Host Facts](../concepts/facts.md#facts-and-the-fleet-server).
