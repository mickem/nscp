#### About `check_secureboot`

`check_secureboot` reports whether **UEFI Secure Boot** is enabled, read from the
registry value
`HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State\UEFISecureBootEnabled`
(the same source `Confirm-SecureBootUEFI` uses). It returns a single result.

Default threshold: **critical** if `enabled = 0`. On legacy-BIOS machines the
value is absent, so both `enabled` and `supported` are 0 — use `supported` to
tell "off" apart from "not applicable" on mixed fleets.

#### On Linux

On Linux the check reads the UEFI global variable
`SecureBoot-8be4df61-93ca-11d2-aa0d-00e098032b8c` from efivarfs
(`/sys/firmware/efi/efivars`; the file is 4 attribute bytes followed by the
value byte), falling back to the older `/sys/firmware/efi/vars/.../data`
interface. Both are world-readable, so the agent needs no privilege. The same
row comes back as on Windows:

- **No `/sys/firmware/efi`** — the host did not boot through UEFI: a legacy
  BIOS boot, `supported=0 enabled=0`, exactly as on Windows. A container that
  masks `/sys/firmware` (Docker does by default) looks the same; run the agent
  on the host to see the firmware.
- **The variable is present** — `supported=1`, `enabled=1` when its value is 1.
- **EFI variables are readable but there is no `SecureBoot` variable** —
  UEFI firmware without Secure Boot support: `supported=0 enabled=0`.
- **The variables cannot be read** (efivarfs is not mounted on
  `/sys/firmware/efi/efivars`, an I/O error, or a truncated variable) — the
  check returns **UNKNOWN** with the reason rather than reporting a guessed
  "disabled".

`setup_mode` reports the `SetupMode` variable: 1 when no Platform Key is
enrolled, so Secure Boot is not enforced and keys can be enrolled without
authentication. It is `unknown` on Windows (not read there), on legacy BIOS
boots and when the firmware does not expose it, and `unknown` never satisfies
a numeric threshold.

macOS has no UEFI variable interface and reports "not supported".
