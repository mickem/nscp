---
icon: "✉️"
modules: [SMTPClient]
---
**SMTP notifications now announce this host in EHLO instead of
`localhost`.** The sender's host name was read from the wrong place, so it
was always empty and the EHLO fell back to `localhost`. If your mail server
applies HELO/EHLO policy (SPF checks, or a rule that rejects `localhost`),
the agent will now identify itself properly - set `ehlo-hostname` on the
target if you need a specific name.
