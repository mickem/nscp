#### About `check_bitlocker`

`check_bitlocker` reports the BitLocker protection state of each encryptable
volume (WMI `Win32_EncryptableVolume` in
`root\CIMV2\Security\MicrosoftVolumeEncryption`). Use it to confirm disk
encryption is actually on where policy requires it.

Default threshold: **critical** if any volume has `protected = 0`. Filter to the
volumes you care about (e.g. `filter=drive = 'C:'`) to avoid alerting on
recovery/utility partitions. **Windows only.** Reading this class requires
elevation, so the agent service must run with sufficient privilege.
