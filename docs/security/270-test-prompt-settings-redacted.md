---
title: "Sensitive settings are redacted in the nscp test settings dump"
fixed_in: next
severity: "Low"
modules: [core]
action: none
---
The `settings` command of the `nscp test` prompt listed the configured
settings with their values in clear text, including keys a module registered
as sensitive with `add_password`. The REST read paths and
`nscp settings --list` were changed to answer `***` for those keys in 0.16.2
([Settings values for sensitive keys are redacted on
read](#settings-values-for-sensitive-keys-are-redacted-on-read)); this
listing was not, and it is the one most likely to be pasted into a ticket or
a chat window.

It now redacts the same keys. As with the other read paths, only keys a
loaded module has declared sensitive are covered, and a module reading its
own configuration is unaffected. This is a defense-in-depth change, not an
authorization boundary: the values are still stored in plaintext in
`nsclient.ini` and readable by anyone who can read that file.

**What to do:** nothing required. Read the value out of `nsclient.ini` when
you need the secret itself.
