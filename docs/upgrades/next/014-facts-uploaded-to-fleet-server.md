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
* A rejected upload (400, 401, 404, 429, 5xx) is retried after 1 minute, then
  2, doubling up to once an hour. No upload goes out in a poll cycle whose
  poll or report got an error answer, and a `Retry-After` on any fleet call
  holds every upload until it passes. A document the server acknowledged and
  then reports missing is re-sent at once the first time (logged at info),
  then after 1 minute, doubling up to once an hour (logged as an error when it
  gets there). A 413 is not retried until the document changes, and the log
  names the largest sets.
* The server must hash the `facts` value exactly as it received it. A server
  that acknowledges a document and then answers with a hash of its own is sent
  it once more, then not again until it changes, and the agent logs an error.
* `[/settings/facts] max size` is re-read on every settings reload. Lowering
  it under the current document drops the largest sets (each is logged) until
  the rest fits.

To keep a set on the host but off the server, there is no separate switch:
turn the set off, or do not enroll the host. See
[Host Facts](../concepts/facts.md#facts-and-the-fleet-server).
