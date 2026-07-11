**Default check:**

```
check_os_Version
L     client CRITICAL: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;50;50
```

Making sure the OS version is **Windows 8**:

```
check_os_Version "warn=version < 62"
L     client WARNING: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;62;0
```

Default check **via NRPE**:

```
check_nrpe --host 192.168.56.103 --command check_os_version
Windows 2012 (6.2.9200)|'version'=62;50;50
```

**Kernel version and architecture** (the default output is now
`${version} (${kernel_version}) ${arch}`, where `kernel_version` is the full
`major.minor.build.ubr`):

```
check_os_version
OK: Windows 11 23H2 (10.0.22631.3810) x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```

Alert on a minimum patch level using `ubr`, and assert a 64-bit fleet:

```
check_os_version "warn=ubr < 3800" "crit=arch != 'x64'"
OK: Windows 11 23H2 (10.0.22631.3810) x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```

**Inventory pull** — BIOS serial / version / manufacturer via a custom
`detail-syntax` (these fields never alert and are empty if WMI is unavailable):

```
check_os_version "detail-syntax=${serial} / ${manufacturer} BIOS ${bios_version} / ${kernel_version} ${arch}"
OK: 5CG1234ABC / American Megatrends Inc. BIOS 1.7.0 / 10.0.22631.3810 x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```

