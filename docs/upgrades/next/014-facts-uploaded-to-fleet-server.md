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
  only when that answer differs from the agent's hash. A server whose poll
  answers carry no such header is never sent the document. The server sends
  the header on its answer to an upload too.
* A rejected upload (any error status but 413) is retried after 1 minute, then
  2, doubling up to once an hour. No upload goes out in a poll cycle whose
  poll or report got an error answer, and a `Retry-After` on a report or an
  upload holds every upload until it passes. A 429 on a poll is waited out
  before the agent calls again; any other failed poll, a 503 included, is
  logged and backed off with its `Retry-After` as the shortest wait. An upload
  cut by the network while the polls get through is paced like a rejection. A document the server
  confirmed holding and then reports missing is re-sent at once the first time
  (logged at info), then after 1 minute, doubling up to once an hour (logged
  as an error when it gets there); one it has not confirmed yet is not re-sent
  for a minute. A 413 is not retried for a day or until the document changes,
  and the log names the largest sets.
* The server must hash the `facts` value exactly as it received it. A server
  that answers an upload with a new hash of its own, or that acknowledges one
  document three times without ever reporting holding it (repeating what it
  held before the upload counts as not reporting), is not sent that document
  again until it changes, the server's reported hash changes, or a day has
  passed, and the agent logs an error.
* `[/settings/facts] max size` is re-read on every settings reload, a
  settings-only one included. Lowering
  it under the current document drops the largest sets (each is logged) until
  the rest fits.

To keep a set on the host but off the server, there is no separate switch:
turn the set off, or do not enroll the host. See
[Host Facts](../concepts/facts.md#facts-and-the-fleet-server).
