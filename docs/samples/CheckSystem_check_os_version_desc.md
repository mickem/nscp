Reports the version of the underlying Windows OS, sourced from the OS version
information, the registry (UBR), and `GetNativeSystemInfo` for the processor
architecture.

| Keyword          | Description                                                                                                                                                                                                    |
|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `version`        | System version (numeric for thresholds, e.g. `major*10+minor`; friendly product name in output, e.g. `Windows 11 23H2`).                                                                                       |
| `major`          | Major version number (perf).                                                                                                                                                                                   |
| `minor`          | Minor version number (perf).                                                                                                                                                                                   |
| `build`          | Build number (perf).                                                                                                                                                                                           |
| `ubr`            | Update Build Revision — the patch level within a build (the `.3803` in `10.0.19045.3803`). Read from the registry; `0` when unavailable (pre-Windows 10).                                                      |
| `kernel_version` | NT kernel version shorthand as `major.minor.build.ubr` (on Windows the kernel version tracks the OS version).                                                                                                  |
| `arch`           | Native processor architecture: `x64`, `x86`, `arm64`, `arm`, `ia64` or `unknown`. Reported via `GetNativeSystemInfo`, so a 32-bit agent under WOW64 still reports the true hardware architecture (e.g. `x64`). |
| `suite`          | Installed suites (Datacenter Edition, Enterprise Edition, Terminal Services, …).                                                                                                                               |

The default warning/critical thresholds (`version <= 50`, i.e. pre-Windows-XP)
exist only to flag ancient/unsupported platforms; they never trip on a supported
OS. Set your own threshold on `build`/`ubr` to alert on a minimum patch level, or
filter on `arch` to assert a fleet's architecture.
