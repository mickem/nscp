#### About `check_firewall`

`check_firewall` reports the **effective** state of the **Windows firewall
profiles** (Domain, Private, Public) — what Windows actually enforces. It reads
the local store through the `INetFwPolicy2` COM interface (no WMI needed) and
then overlays any Group Policy resultant values (the ones the policy engine
writes under `HKLM\SOFTWARE\Policies\Microsoft\WindowsFirewall`): a setting
enforced through group policy wins over the local setting, exactly as the
firewall service applies it. Both the modern per-profile policy keys and the
legacy `StandardProfile` key (written by the pre-Vista "Protect all network
connections" ADMX policy, still honoured and applying to both Private and
Public) are read; when both are present the modern values win as a whole.
This matches
`Get-NetFirewallProfile -PolicyStore ActiveStore` — note that plain
`Get-NetFirewallProfile` (and `INetFwPolicy2` alone) shows only the local,
pre-policy configuration.

Each profile is one row with these keywords:

| Keyword    | Type   | Meaning                                                                |
|------------|--------|------------------------------------------------------------------------|
| `profile`  | string | Profile name: `Domain`, `Private` or `Public`.                         |
| `enabled`  | bool   | True if the firewall is enabled for that profile. Emitted as perfdata. |
| `active`   | bool   | True if the profile is currently applied to a connected network. Emitted as perfdata (`<profile> active`). |
| `inbound`  | string | Default inbound action (`allow`/`block`).                              |
| `outbound` | string | Default outbound action (`allow`/`block`).                             |
| `policy`   | string | Where the profile's settings come from: `group policy` if any of the reported settings is enforced through group policy, otherwise `local`. |

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
