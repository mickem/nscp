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

* Every desired-state poll (`facts_hash=` query parameter) and every state
  report (`facts_hash` member) carries the SHA-256 of the facts document,
  never the document itself.
* The server answers with the hash it holds in an `X-Facts-Hash` response
  header. The document is uploaded on its own call, `POST /agent/v1/facts`,
  only when that answer differs from the agent's hash. A server that sends no
  such header is never sent the document.
* An upload is capped by `[/settings/facts] max size`. A document over it, or
  one the server refuses, is not sent again until it changes, and the log
  names the largest sets. A server that keeps reporting a miss for a document
  it acknowledged gets it again at most hourly, after a short backoff.

To keep a set on the host but off the server, there is no separate switch:
turn the set off, or do not enroll the host. See
[Host Facts](../concepts/facts.md#facts-and-the-fleet-server).
