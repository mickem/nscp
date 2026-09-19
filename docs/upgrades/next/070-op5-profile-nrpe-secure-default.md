---
icon: "🔒 💥"
modules: [NRPEServer, packaging]
action: conditional
---
**The op5 installer profile defaults to secure NRPE.** Picking **op5** on the
MSI's monitoring-tool page used to force NRPE into legacy mode — anonymous
Diffie-Hellman, no peer verification — hide the mode radio group so the choice
could not be seen or changed, and enable `allow nasty characters` for every
NRPE-reachable check. The profile now defaults to the same secure mode the
generic profile uses (`verify mode = peer-cert`, `insecure = false`), shows the
NRPE mode radio group so legacy is a visible decision, and no longer sets
`allow nasty characters`. `allow arguments` stays on, because op5's check
commands need it; what changes is that a caller has to present a certificate
first. An op5 host monitored by a `check_nrpe` with no client certificate needs
either a certificate or **Insecure mode** picked explicitly on that page. See
the
[security notice](../security/notices.md#the-op5-installer-profile-no-longer-forces-insecure-nrpe).
