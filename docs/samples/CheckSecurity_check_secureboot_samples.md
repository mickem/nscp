**Check that UEFI Secure Boot is enabled (Windows)**

The default is critical if Secure Boot is not enabled.

```
check_secureboot
L        cli OK: secure boot enabled=1 supported=1
```

```
check_secureboot
L        cli CRITICAL: secure boot enabled=0 supported=1
```

**Distinguish "disabled" from "not supported" (legacy BIOS)**

`supported` is 0 when the platform does not expose a Secure Boot state (legacy
BIOS boot). Treat that as WARNING rather than CRITICAL if you monitor mixed
fleets:

```
check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
L        cli WARNING: secure boot enabled=0 supported=0
```

**Linux: a legacy BIOS boot (or a container without `/sys/firmware`)**

With no `/sys/firmware/efi` the host has no Secure Boot to enable, so the
default critical trips; exempt it with `supported`, as on Windows:

```
check_secureboot
CRITICAL: secure boot enabled=0 supported=0|'secureboot_enabled'=0;;0
```

```
check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
WARNING: secure boot enabled=0 supported=0|'secureboot_enabled'=0;;0 'secureboot_supported'=0;0;1
```

**Linux: alert on firmware left in Setup Mode**

`setup_mode` is read from the UEFI `SetupMode` variable; it is `unknown` where
it cannot be known (here a BIOS boot, and always on Windows):

```
check_secureboot "crit=setup_mode = 1" "detail-syntax=enabled=${enabled} supported=${supported} setup_mode=${setup_mode}"
OK: enabled=0 supported=0 setup_mode=unknown
```

**On macOS**

There is no Darwin reader yet: the check returns UNKNOWN with
`check_secureboot is not supported on this platform (Windows and Linux UEFI only)`.
