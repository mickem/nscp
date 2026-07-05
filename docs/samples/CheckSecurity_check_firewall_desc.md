#### About `check_firewall`

`check_firewall` reports the state of the **Windows firewall profiles**
(Domain, Private, Public), the same data `Get-NetFirewallProfile` exposes. It
reads them through the `INetFwPolicy2` COM interface, so it needs no WMI.

Each profile is one row with these keywords:

| Keyword    | Type   | Meaning                                                                |
|------------|--------|------------------------------------------------------------------------|
| `profile`  | string | Profile name: `Domain`, `Private` or `Public`.                         |
| `enabled`  | bool   | True if the firewall is enabled for that profile. Emitted as perfdata. |
| `active`   | bool   | True if the profile is currently applied to a connected network. Emitted as perfdata (`<profile> active`). |
| `inbound`  | string | Default inbound action (`allow`/`block`).                              |
| `outbound` | string | Default outbound action (`allow`/`block`).                             |

Default threshold: **critical** if any profile has `enabled = 0`.

`active` reflects which profile(s) Network Location Awareness currently
applies (`INetFwPolicy2::CurrentProfileTypes`). More than one profile can be
active when several networks are connected; with **no** connected network
Windows reports the Public profile as active. Its main use is catching a
machine silently dropping from `Domain`/`Private` to `Public` after a
router/adapter change — firewall rules scoped to the domain or private
profile stop applying and services start getting blocked. This is opt-in via
a `warn`/`crit` expression (see the samples) since being on the public
profile is perfectly normal for e.g. laptops.

This check is **Windows only**. It models Windows' fixed three-profile firewall,
which does not map onto Linux firewalls (firewalld zones, ufw, nftables/iptables
default policies); on non-Windows platforms it returns UNKNOWN with a clear
message rather than pretending to check something equivalent.
