#### About `check_firewall`

`check_firewall` reports the state of the **Windows firewall profiles**
(Domain, Private, Public), the same data `Get-NetFirewallProfile` exposes. It
reads them through the `INetFwPolicy2` COM interface, so it needs no WMI.

Each profile is one row with these keywords:

| Keyword    | Type   | Meaning                                                                |
|------------|--------|------------------------------------------------------------------------|
| `profile`  | string | Profile name: `Domain`, `Private` or `Public`.                         |
| `enabled`  | bool   | True if the firewall is enabled for that profile. Emitted as perfdata. |
| `inbound`  | string | Default inbound action (`allow`/`block`).                              |
| `outbound` | string | Default outbound action (`allow`/`block`).                             |

Default threshold: **critical** if any profile has `enabled = 0`.

This check is **Windows only**. It models Windows' fixed three-profile firewall,
which does not map onto Linux firewalls (firewalld zones, ufw, nftables/iptables
default policies); on non-Windows platforms it returns UNKNOWN with a clear
message rather than pretending to check something equivalent.
