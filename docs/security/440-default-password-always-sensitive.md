---
title: "The shared default password is always treated as sensitive"
fixed_in: 0.22.0
severity: "Low"
modules: [core]
action: none
---
Redaction of sensitive settings is per registered key: a module declares a key
with `add_password`, and the core then answers `***` for it on every read path
([Settings values for sensitive keys are redacted on
read](#settings-values-for-sensitive-keys-are-redacted-on-read), [Sensitive
settings are redacted in the nscp test settings
dump](#sensitive-settings-are-redacted-in-the-nscp-test-settings-dump)).

`password` under `/settings/default` is the shared secret the NRPE, NSCA and
NSClient protocols and the web server all fall back to, but it is declared
only by `WEBServer`, `NSCAServer` and `NSClientServer`. On an agent running
none of those — check modules only, or `NRPEServer` alone, which reads the key
but does not declare it — nothing marked it sensitive, so the value was
printed in clear text by the `settings` command of the `nscp test` prompt, by
`nscp settings --list` / `--show`, and by the REST read paths. Which modules
happened to be enabled decided whether the same secret was masked.

The core now registers the key as sensitive itself, so the masking no longer
depends on the module list. Sensitivity is still an exact path-and-key entry:
`allowed hosts` beside it, and a `password` under some other path, are
unaffected. As before, this is a defense-in-depth change and not an
authorization boundary — the value is still stored in `nsclient.ini` and
readable by anyone who can read that file.

**What to do:** nothing required. See the [upgrade
note](../setup/upgrading.md) if you store credentials in the Windows
Credential Manager.
