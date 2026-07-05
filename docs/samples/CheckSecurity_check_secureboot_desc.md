#### About `check_secureboot`

`check_secureboot` reports whether **UEFI Secure Boot** is enabled, read from the
registry value
`HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State\UEFISecureBootEnabled`
(the same source `Confirm-SecureBootUEFI` uses). It returns a single result.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `enabled` | bool | True if UEFI Secure Boot is enabled. |
| `supported` | bool | True if the platform exposes a Secure Boot state (UEFI); false on legacy BIOS boots. |

Default threshold: **critical** if `enabled = 0`. On legacy-BIOS machines the
value is absent, so both `enabled` and `supported` are 0 — use `supported` to
tell "off" apart from "not applicable" on mixed fleets. **Windows only.**
