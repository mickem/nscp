---
icon: "🧹"
modules: [GraphiteClient]
---
**The Graphite `retry` setting is not honoured and is no longer read.**
The module always made exactly one attempt per submission; reading the value
made it look otherwise, and a retry loop would multiply the worst-case time
a stalled endpoint can hold the submitting thread by the retry count.
`retry`/`retries` are registered for every client module centrally, so they
still appear in the GraphiteClient reference, but `GraphiteClient` does not
act on them. Mirrors the SMTP `retry` change in 0.18.0.
