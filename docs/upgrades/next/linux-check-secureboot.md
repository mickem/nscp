---
icon: "🐧"
modules: [CheckSecurity]
action: none
---
**`check_secureboot` now works on Linux.** It used to report "not supported"
off Windows. It reads the UEFI `SecureBoot` variable from `/sys/firmware/efi`:
no EFI directory is a legacy BIOS boot (`supported=0`, as on Windows), and
variables that cannot be read are UNKNOWN rather than "disabled". A new
`setup_mode` keyword reports UEFI Setup Mode (`unknown` on Windows). Windows
behaviour is unchanged; macOS still reports "not supported".
