---
icon: "🧹"
modules: [SMTPClient]
action: none
---
**The SMTP `retry` setting is not honoured and is no longer read.** The
module always made exactly one attempt per submission; reading the value
made it look otherwise. `retry`/`retries` are registered for every client
module centrally, so they still appear in the SMTPClient reference, but
`SMTPClient` does not act on them.
