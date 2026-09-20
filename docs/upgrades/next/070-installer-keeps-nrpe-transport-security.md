---
icon: "🔧"
modules: [NRPEServer, packaging]
action: conditional
---
**The Windows installer no longer rewrites NRPE transport security it did not
configure.** The MSI offers two NRPE presets, *insecure* and *secure*, and used
to apply one whenever it could not recognise the mode an existing or imported
configuration was in. A configuration that ran TLS without client certificates
matched neither preset, so `msiexec /i NSCP-….msi IMPORT_CONFIG=…` came back
with four keys under `[/settings/NRPE/server]` that the imported file never
asked for: `verify mode` forced to `peer-cert`, `tls version` lowered to
`tlsv1.2+`, plus `insecure = false` and an empty `ssl options`
([#1558](https://github.com/mickem/nscp/issues/1558)). A configuration whose
mode *was* recognised kept its mode but still had those last two pairs
rewritten.

The installer now records what it found, and writes a preset only where the
mode actually changes — a fresh install, or an operator who moves the radio
button or passes `NRPEMODE=LEGACY` / `NRPEMODE=SECURE` on the command line. It
also no longer writes `tls version` or `ssl options` at all: both were being set
to the value `NRPEServer` already defaults to, so they added nothing to a new
install while overwriting an explicit choice on an existing one.

Check `[/settings/NRPE/server]` after upgrading if you install or upgrade on
Windows with an NRPE configuration of your own, in particular one imported with
`IMPORT_CONFIG`:

* `verify mode` is now left as your configuration has it. If a 0.21.0 install
  silently moved you to `peer-cert` and you have since issued client
  certificates, keep the setting by leaving it in your configuration — it will
  no longer be re-added for you.
* `tls version` is left alone, so a pinned `tls1.3` survives the install
  instead of being replaced with `tlsv1.2+`.
