---
icon: "🔒 🔧"
modules: [CheckLogFile, CheckWMI, CheckSystem]
action: none
---
**`check_logfile`, `check_wmi` and `check_pdh` can now be told which files,
queries and counters a caller may ask for.** Nothing to do on upgrade: all
three default to `any`, which is exactly what earlier releases did. Those
checks take an argument that decides *what data is read*, and the agent reads
it with its own privileges — so where callers choose the argument (NRPE with
`allow arguments = true`, or the REST API) an unrestricted `file=` is a general
file-read primitive. Each module gained a mode setting and an allow list:

| Check | Section | Mode setting | Allow list |
|-------|---------|--------------|------------|
| `check_logfile` | `[/settings/logfile]` | `file access` | `allowed files` |
| `check_wmi` | `[/settings/wmi]` | `query access` | `allowed classes`, `allowed namespaces` |
| `check_pdh` | `[/settings/system/windows]` | `counter access` | `allowed counters` |

The modes are `any`, `allowed` (only what matches the list) and `predefined`
(only names you configured — `[/settings/logfile/files]`,
`[/settings/wmi/queries]`, and for `check_pdh` the counters already in
`[/settings/system/windows/counters]`). Configured names resolve in every mode,
so you can name your checks first and tighten the mode afterwards. Two things
only take effect once a mode is set: a `check_wmi` `namespace=` may then no
longer be moved off `root\cimv2` unless `allowed namespaces` says so, and
`target=` must name a target defined in `[/settings/wmi/targets]`. See
[Restricting what a check may read](../concepts/check-access.md), the
[securing guide](securing.md#data-disclosure-restricting-what-a-check-may-read)
and the
[security notice](../security/notices.md#check-access-modes-for-check_logfile-check_wmi-and-check_pdh).
