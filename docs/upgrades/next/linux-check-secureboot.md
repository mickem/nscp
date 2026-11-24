---
icon: "🐧"
modules: [CheckSecurity]
action: conditional
---
**`check_secureboot` now works on Linux.** It used to report "not supported"
off Windows. It reads the UEFI `SecureBoot` variable from `/sys/firmware/efi`:
no EFI directory is a legacy BIOS boot (`supported=0`, as on Windows), and
variables that cannot be read are UNKNOWN rather than "disabled". A new
`setup_mode` keyword reports UEFI Setup Mode (`unknown` on Windows). On both
platforms an OK result now prints the state (`OK: secure boot enabled=1
supported=1`) instead of the fixed text `OK: secure boot is enabled`, which was
also printed when a threshold excused a host with Secure Boot off; a plugin
output match on the old text needs updating. macOS still reports "not
supported".
