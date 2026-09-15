---
icon: "🔒"
modules: [core]
action: conditional
---
**The shared `/settings/default/password` is always redacted, and follows
`use credentials`.** Nothing to do on a default install. The key is now
registered sensitive by the core rather than by whichever of `WEBServer`,
`NSCAServer` and `NSClientServer` happens to be loaded, so `nscp test`'s
`settings` dump, `nscp settings --list` / `--show` and the REST read paths
answer `***` for it on every agent instead of only those running one of those
modules. Read the value out of `nsclient.ini` when you need the secret itself.
Two consequences if you rely on the old behaviour:

* Tooling that scraped the shared password back out of one of those listings
  now receives `***`.
* On Windows with `use credentials = true`, the next write of the key (a
  `nscp settings --update`, or any change made through the REST or CLI settings
  writers) moves it into the Credential Manager and leaves a reference in
  `nsclient.ini`, the same as every other sensitive key. Agents that do not
  enable credentials are unaffected, and the key is not rewritten until
  something writes it.

See the [security notice](../security/notices.md#the-shared-default-password-is-always-treated-as-sensitive).
