---
icon: "🧪 🌐"
modules: [CheckWindowsApps]
action: none
---
**New checks: `check_dhcp_scopes` and `check_dhcp_server` (Windows,
experimental).** Nothing to do on an upgrade. On a Windows DHCP server with
`CheckWindowsApps` enabled, `check_dhcp_scopes` reports the address
utilisation of every IPv4 scope (addresses in use, free and offered, and
`used_pct`), and `check_dhcp_server` reports the server's packet rates and
queue lengths from the *DHCP Server* performance counters.
`check_dhcp_scopes` warns at `used_pct > 80` and goes critical at
`used_pct > 95`, over the enabled scopes only; `check_dhcp_server` warns on any
DHCPDECLINE (an address conflict), on more than 10 NAKs a second or a queue of
more than 50 packets, and goes critical at a queue of 200. On a host without
the DHCP Server role both checks return UNKNOWN with a message that says so.
The default LocalSystem account can read the server; any other service account
needs *DHCP Users* membership. `dhcpsapi.dll` is delay-loaded, so the other
`CheckWindowsApps` checks never depend on it. Like the rest of the module, the
checks are marked experimental: their options, keywords and output may still
change.
