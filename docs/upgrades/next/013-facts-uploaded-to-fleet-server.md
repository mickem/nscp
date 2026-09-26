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
* A server that rejects the upload (400, 401, 429, 5xx), or keeps reporting
  a miss for a document it acknowledged, is retried after 1 minute, doubling
  up to once an hour. A document the server refuses outright (413, or 404) is
  not sent again until it changes, and the log names the largest sets.

To keep a set on the host but off the server, there is no separate switch:
turn the set off, or do not enroll the host. See
[Host Facts](../concepts/facts.md#facts-and-the-fleet-server).
