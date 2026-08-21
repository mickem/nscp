Reports the version of the underlying Windows OS, sourced from the OS version
information, the registry (UBR), `GetNativeSystemInfo` for the processor
architecture, and `Win32_BIOS` (WMI) for the inventory fields.

The default warning/critical thresholds (`version <= 50`, i.e. pre-Windows-XP)
exist only to flag ancient/unsupported platforms; they never trip on a supported
OS. Set your own threshold on `build`/`ubr` to alert on a minimum patch level, or
filter on `arch` to assert a fleet's architecture.

`serial`, `bios_version` and `manufacturer` are **inventory-only**: they are read
best-effort from WMI, are empty when WMI is unavailable, are not part of the
default output, and are not intended for alerting. Reference them in a custom
`detail-syntax` (or `top-syntax`) to pull inventory.
