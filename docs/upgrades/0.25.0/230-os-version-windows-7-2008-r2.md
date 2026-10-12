---
icon: "🔧"
modules: [CheckSystem]
action: conditional
---
**`check_os_version` no longer swaps Windows 7 and Server 2008 R2, and names
Windows 11 25H2.** Since 0.6.15 a Windows 7 machine was reported as
`Windows Server 2008 R2` and a Server 2008 R2 machine as `Windows 7` (in the
check's message and in the `os_name` system fact). Both are now named
correctly, and Windows 11 build 26200 and later reports `Windows 11 25H2`
instead of `24H2`. If a filter or a downstream rule matches on these names,
check it after upgrading.
