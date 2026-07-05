**Check that UEFI Secure Boot is enabled (Windows)**

The default is critical if Secure Boot is not enabled.

```
check_secureboot
L        cli OK: secure boot is enabled
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

**On non-Windows platforms**

```
check_secureboot
L        cli UNKNOWN: check_secureboot is not supported on this platform (Windows/UEFI only)
```
