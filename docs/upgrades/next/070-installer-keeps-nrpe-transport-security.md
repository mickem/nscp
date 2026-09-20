---
icon: "🔧"
modules: [NRPEServer, packaging]
action: conditional
---
**The Windows installer no longer rewrites NRPE transport security it did not
configure.** The MSI offers two NRPE presets, *insecure* and *secure*, and used
to apply one whenever it could not recognise the mode an existing or imported
configuration was in. It recognised only `insecure = true` and
`verify mode = peer-cert`, so two perfectly ordinary setups fell through:

* A listener running TLS **without** client certificates (`verify mode = none`).
  `msiexec /i NSCP-….msi IMPORT_CONFIG=…` came back with four keys under
  `[/settings/NRPE/server]` that the imported file never asked for: `verify mode`
  forced to `peer-cert`, `tls version` lowered to `tlsv1.2+`, plus
  `insecure = false` and an empty `ssl options`
  ([#1558](https://github.com/mickem/nscp/issues/1558)).
* A listener configured **without either key** — `port = 5666` and nothing more,
  running on `NRPEServer`'s own defaults. An upgrade read that as "nothing
  configured here" and wrote the secure preset, so a listener that had never
  required client certificates started demanding them.

A configuration whose mode *was* recognised kept its mode, but still had
`tls version` and `ssl options` rewritten.

The installer now reads what the configuration says and applies a preset only
where it would overwrite nothing: a fresh install with no NRPE listener
configured, or an operator who moves the radio button or passes
`NRPEMODE=LEGACY` / `NRPEMODE=SECURE` on the command line. It also no longer
writes `tls version` or `ssl options` at all — both were being set to the value
`NRPEServer` already defaults to, so they added nothing to a new install while
overwriting an explicit choice on an existing one.

Check `[/settings/NRPE/server]` after upgrading if you install or upgrade on
Windows with an NRPE configuration of your own, in particular one imported with
`IMPORT_CONFIG`:

* `verify mode` is now left as your configuration has it, and is no longer added
  where you never set it. If an earlier install silently moved you to
  `peer-cert` and you have since issued client certificates, keep it by leaving
  the setting in your configuration — it will not be re-added for you.
* `tls version` is left alone, so a pinned `tls1.3` survives the install instead
  of being replaced with `tlsv1.2+`.
* A fresh install is unchanged: it still gets `verify mode = peer-cert`, or
  `insecure = true` with `NRPEMODE=LEGACY`.
