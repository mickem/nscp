---
title: "A junction defeated the modern-layout shared folder lockdown"
fixed_in: 0.19.0
severity: "Medium"
modules: [packaging, core]
action: conditional
---
Affects the opt-in, experimental `LAYOUT=modern` install property and
`nscp settings --migrate-layout modern` only; legacy installs are untouched.

That layout keeps `nsclient.ini`, the fleet private key and the TLS material in
`%ProgramData%\NSClient++`, and locks the folder down by taking ownership and
replacing its DACL. Every step of that was path-based, and a standard user can
create a junction under that name before the installer first runs. The owner
and DACL were then applied to the junction's *target* while the link itself
stayed theirs to remove and replace — with a crafted `nsclient.ini` in its
place, the next service start loaded that configuration as SYSTEM.

Reparse points are refused now, and ownership and the DACL are applied through
a handle opened on the entry itself, so a junction fails the install, the
migration and service start rather than being secured through.

**What to do:** on the modern layout, replace a junction at the shared folder
with a real directory, or relocate the folder with a `[paths]` override in
`boot.ini`.
