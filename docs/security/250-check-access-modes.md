---
title: "Access modes for the checks whose argument decides what is read"
fixed_in: next
severity: "Low (hardening; no vulnerability — the previous behaviour is the documented purpose of these checks)"
modules: [CheckLogFile, CheckWMI, CheckSystem, CheckDisk, CheckEventLog]
action: conditional
---
Several checks take an argument which decides *what data is read* rather than how
it is judged, and the agent reads it with its own privileges (`SYSTEM` on
Windows, `root` or the service account on Linux):

| Check | Argument | Reach |
|-------|----------|-------|
| `check_logfile` | `file=` | any file the agent can open; the `line` and `columnN` keywords return its contents |
| `check_wmi` | `query=` | any WMI class, including the filesystem via `CIM_DataFile` and `Win32_Directory` |
| `check_pdh` (`check_counter`) | `counter=` | any performance object on the machine |
| `check_files`, `check_single_file` | `path=`, `file=` | any directory tree the agent can read: every name, size and timestamp, and a checksum of any file |
| `check_disk_write` | `file=` | creates and deletes a test file at any path the agent can write |
| `check_registry_key`, `check_registry_value` | `key=` | any registry key, and the value data itself, with binary rendered as hex |
| `check_eventlog` | `file=`, `log=` | any event log channel, and the event text itself |

This is the documented purpose of those checks and is not a vulnerability: on a
host where only the configuration decides what runs, nothing here is reachable
by a caller. It becomes a disclosure surface where the **caller** chooses the
argument — NRPE with `allow arguments = true`, or a REST user not on the
no-arguments [`restricted` role](../setup/securing.md#adding-a-dedicated-user)
— because a single check can then return the contents of any file the agent can
read. Until
now an operator had no way to narrow that short of disabling the module.

#### What changed

Each of these modules gained an access mode and an allow list, defaulting to
`any` — the behaviour of every earlier release, so no installation changes on
upgrade.

| Check | Section | Mode setting | Allow list |
|-------|---------|--------------|------------|
| `check_logfile` | `[/settings/logfile]` | `file access` | `allowed files` |
| `check_wmi` | `[/settings/wmi]` | `query access` | `allowed classes`, `allowed namespaces` |
| `check_pdh` | `[/settings/system/windows]` | `counter access` | `allowed counters` |
| `check_files`, `check_single_file`, `check_disk_write` | `[/settings/disk]` | `file access` | `allowed files` |
| `check_registry_key`, `check_registry_value` | `[/settings/system/windows]` | `registry access` | `allowed registry keys` |
| `check_eventlog` | `[/settings/eventlog]` | `log access` | `allowed logs` |

`allowed` accepts only values matching the list; `predefined` accepts only names
the operator configured and refuses a raw value outright. Configured names
resolve in every mode, so a site can name its checks first and tighten the mode
afterwards without rewriting the monitoring server's commands.

Three details matter for the strength of the control:

* **File paths are resolved before they are matched.** `..` is flattened and
  symbolic links and junctions are followed, so neither a traversal out of an
  allowed directory nor a link planted inside one widens it. The resolved path
  is also the one the check opens, so nothing re-resolves the name between the
  check and the read.
* **A WMI query whose class cannot be determined with certainty is refused, not
  guessed at.** `ASSOCIATORS OF`, `REFERENCES OF`, a class path carrying a
  namespace or machine name, and multiple statements are all rejected in
  `allowed` mode; approving the trailing identifier of a qualified path would
  let the query read from a provider the allow list never permitted. Such
  queries remain usable as predefined queries.
* **A misspelled mode fails closed.** An unrecognised value refuses every
  request and is logged at startup, rather than reading as "no restriction".

The registry and event-log gates cover the two widest reads of the set, and both
return content in their *default* rendering, so a caller needs no syntax argument
to receive it. `check_registry_value` renders `REG_BINARY` as hex and will walk a
whole subtree with `recursive=true`; `check_eventlog` returns event text through
`message`, `strings` and `xml` from any channel the agent can read. Their
allow-list entries are hierarchical rather than glob - an entry covers that key or
channel and everything below it, matched on whole name segments, so
`HKLM\SOFTWARE\MyApp` cannot be widened into `HKLM\SOFTWARE\MyAppOther` - and
both registry hive spellings compare equal. Only the starting key or channel is
checked, because enumeration below it cannot leave the subtree. While registry
access is restricted the `computer=` argument is refused, so the local registry is
the only one reachable.

The disk checks are a weaker case than `check_logfile` and were included for
reach rather than depth: they never return file contents, but they enumerate
whole directory trees (name, size, timestamps, executable version) and their
checksum keywords hash any readable file, which confirms known content and for a
short or predictable file effectively recovers it. For `check_files` only the
scan root is held against the policy - the recursion refuses to follow symbolic
links, directory and file links alike (the Windows scanner previously skipped
only directory reparse points, so a file symlink planted in an allowed tree
could be hashed through), so every file it yields is genuinely beneath a root
which passed.

Refusals name what was rejected and the section that governs it, but never the
contents of the allow list.

While `query access` is not `any`, `check_wmi` additionally requires
`namespace=` to match `allowed namespaces` (an empty list meaning the default
`root\cimv2` only), and requires `target=` to name a target defined in
`[/settings/wmi/targets]` — an unknown target was otherwise taken as a bare host
name, which would let a caller point a restricted check at a machine of its own
choosing.

The `nscp wmi` command line is unchanged: it is a local administrative tool, and
reaching it over REST already requires the `legacy` or `console.exec`
permission, which is administrator-equivalent.

**What to do:** nothing on upgrade. If callers can pass arguments to these
checks — NRPE with `allow arguments = true`, or a REST user not on the
no-arguments `restricted` role — set the mode to `predefined` (or `allowed`) on the modules you have enabled. See
[Restricting what a check may read](../concepts/check-access.md) and the
[securing guide](../setup/securing.md#data-disclosure-restricting-what-a-check-may-read).
