---
icon: "🐛"
modules: [CheckSystem]
action: none
---
**Network and temperature collectors work again on Windows Server 2008 / 2008
R2.** Nothing to do. On Windows versions older than 8 / Server 2012 the
network collector failed every second with
`Failed to get network metrics: ... WinMgmt - :failed to lookup error code: 2147749904`
because it also queried `Win32_PerfRawData_Tcpip_NetworkAdapter`, which those
versions do not have; `check_network` had no data. It now reports the
`interface` source there and quietly stops asking for the `adapter` source, so
`source=adapter` rows (which carry NIC-team aggregates) are absent on those
hosts. The temperature collector logged the same error every second when
neither thermal-zone WMI class exists; it now logs
`Failed to fetch temperature metrics (neither ACPI nor performance counter source available), disabling...`
once and `check_temperature` reports `No temperature sensors found`.
