#### About `check_bitlocker`

`check_bitlocker` reports the BitLocker protection state of each encryptable
volume (WMI `Win32_EncryptableVolume` in
`root\CIMV2\Security\MicrosoftVolumeEncryption`). Use it to confirm disk
encryption is actually on where policy requires it.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `drive` | string | Drive letter (may be empty for non-lettered volumes). |
| `protected` | bool | True when `protection_status == 1` (BitLocker on). |
| `protection_status` | int | Raw status: 0 off, 1 on, 2 unknown. |
| `conversion_status` | int | Raw conversion status (0 decrypted, 1 encrypted, …). |

Default threshold: **critical** if any volume has `protected = 0`. Filter to the
volumes you care about (e.g. `filter=drive = 'C:'`) to avoid alerting on
recovery/utility partitions. **Windows only.** Reading this class requires
elevation, so the agent service must run with sufficient privilege.
