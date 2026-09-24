---
title: "The op5 installer profile no longer forces insecure NRPE"
fixed_in: 0.23.0
severity: "Medium for hosts installed with the op5 profile"
modules: [NRPEServer, packaging]
action: conditional
---
Choosing **op5** on the MSI's monitoring-tool page did four things nobody was
shown. The installer forced the NRPE mode to `LEGACY`, the configuration page
hid and disabled the NRPE mode radio group so the choice could not be seen or
changed, and the profile's `op5.ini` pinned `verify mode = none` and
`insecure = true` on top — overriding the mode even where the property had been
set. It also enabled `allow arguments` and `allow nasty characters` for the
NRPE server.

The result was an NRPE listener on anonymous Diffie-Hellman with no peer
verification, authenticated by the `allowed hosts` IP list alone, accepting
caller-supplied arguments with the shell metacharacter guard turned off. That
is the combination the project's own documentation warns against, arrived at by
picking a vendor name on an installer page.

In this release:

* The op5 profile defaults to the same `SECURE` NRPE mode the generic profile
  uses: `insecure = false` and `verify mode = peer-cert`, so a caller has to
  present a certificate that chains to the configured CA.
* The NRPE checkbox and the NRPE mode radio group are shown and enabled for op5
  installs, so `LEGACY` is a visible decision rather than a hidden default.
* `op5.ini` no longer pins `verify mode` or `insecure` at all — the mode chosen
  on the page is what applies — and no longer sets
  `allow nasty characters = true`.

`allow arguments = true` stays in the profile, because op5's server-side check
commands pass arguments to the built-in checks and removing it would break
monitoring rather than harden it. What changes is who may pass them: with the
secure mode as the default, the caller has to hold a certificate first.

**What to do:** this affects new installs and upgrades that re-run the
configuration page. An op5 host monitored by a `check_nrpe` that cannot present
a client certificate must either be given one (point `ca` at your monitoring
CA) or be installed with **Insecure mode** picked explicitly on the
configuration page. Existing installations keep the configuration already
written to the registry until it is changed. If you relied on
`allow nasty characters` for an op5 check, set it yourself in `nsclient.ini`
and understand what it disables.
