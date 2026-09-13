# Restricting what a check may read

Most checks judge something the agent already knows: how full a disk is, whether
a service is running. A few take an argument which decides *what data is read in
the first place*:

| Check | Argument | What it reaches |
|-------|----------|-----------------|
| `check_logfile` | `file=` | any file the agent can open, returned through the `line` and `columnN` keywords |
| `check_wmi` | `query=` | any WMI class, including the filesystem via `CIM_DataFile` and `Win32_Directory` |
| `check_pdh` (`check_counter`) | `counter=` | any performance object on the machine |
| `check_files`, `check_single_file` | `path=`, `file=` | any directory tree the agent can read: every name, size and timestamp, plus a checksum of any file |
| `check_disk_write` | `file=` | creates and deletes a test file at any path the agent can write |
| `check_registry_key`, `check_registry_value` | `key=` | any registry key, and the value data itself, with binary rendered as hex |
| `check_eventlog` | `file=`, `log=` | any event log channel, and the event text itself |

That is what those checks are for, and on a host where only the configuration
decides what runs, it is not a problem. It becomes one where the *caller*
chooses the argument — NRPE with `allow arguments = true`, or a REST user who
is not on the no-arguments [`restricted` role](../setup/securing.md#adding-a-dedicated-user) —
because the agent runs as `SYSTEM` (Windows) or `root`/`nsclient` (Linux) and
the argument then decides how much of the machine a single check can read back.

An operator who only wants two log files watched has no reason to leave the
other several hundred thousand reachable. The access-mode settings are how you
say so.

!!! note "Nothing changes unless you change it"

    Every one of these defaults to `any`, which is what each release before
    0.21.0 did. Restricting access is opt-in; upgrading does not break a
    working setup.

## The three modes

Each module has one mode setting and, where it applies, one allow list:

| Check | Section | Mode setting | Allow list |
|-------|---------|--------------|------------|
| `check_logfile` | `[/settings/logfile]` | `file access` | `allowed files` |
| `check_wmi` | `[/settings/wmi]` | `query access` | `allowed classes`, `allowed namespaces` |
| `check_pdh` | `[/settings/system/windows]` | `counter access` | `allowed counters` |
| `check_files`, `check_single_file`, `check_disk_write` | `[/settings/disk]` | `file access` | `allowed files` |
| `check_registry_key`, `check_registry_value` | `[/settings/system/windows]` | `registry access` | `allowed registry keys` |
| `check_eventlog` | `[/settings/eventlog]` | `log access` | `allowed logs` |

=== "any"

    Anything the caller names is read. The default, and the behaviour of every
    earlier release.

=== "allowed"

    Only values matching the allow list are read. Everything else is refused
    with a message naming what was rejected — never what the list contains.

=== "predefined"

    Only names *you* defined in configuration are accepted. A raw path, query or
    counter path from the caller is refused outright. This is the tightest
    setting, and the one to reach for when the monitoring server should be able
    to run your checks and nothing else.

### Predefined names work in every mode

A name you configured always resolves, whatever the mode is set to — `any`
included. That ordering is deliberate: it lets you name your checks first,
confirm the monitoring server still works, and tighten the mode afterwards,
instead of having to do both in one step.

It also means a predefined value is trusted as written. A predefined WMI query
is not parsed and is not held against `allowed classes`; you configured it, so
you have vouched for it.

## check_logfile

```ini
[/settings/logfile]
file access = allowed
allowed files = C:/logs, C:/inetpub/logs/LogFiles/**.log

; or, tighter:
file access = predefined

[/settings/logfile/files]
app = C:/logs/app.log
iis = C:/inetpub/logs/LogFiles/W3SVC1/u_ex.log
```

With the section above and `file access = predefined`, the monitoring server
runs `check_logfile file=app` and nothing else.

Allow-list entries come in three shapes:

| Entry | Matches |
|-------|---------|
| `C:/logs` | a directory: every file beneath it, at any depth |
| `C:/logs/*.log` | a wildcard (`*` and `?`), matched against the whole path |
| `C:/logs/app.log` | that one file |

Paths are compared after both sides are resolved, and separators are folded
only where the platform treats them as separators: on Windows `\` and `/` are
the same thing, on Linux `\` is an ordinary character in a file name. An entry
may also be a filesystem root (`/`, or `C:\`), which allows everything on it.

A bare entry is a directory unless it names a file which exists when the
settings are read; a path which does not exist yet (a volume mounted later, a
log directory the application creates on first run) is taken as a directory,
so the files that appear beneath it are covered without a reload. In a
wildcard, `*` and `?` stop at a directory separator: `C:/logs/*.log` names the
`.log` files in `C:/logs` itself and not those in `C:/logs/private/`. Write
`**` where the whole subtree is meant, as in `C:/logs/**.log`.

**Paths are resolved before they are matched.** `..` is flattened and symbolic
links and junctions are followed, so neither
`C:/logs/../../Windows/System32/config/SAM` nor a link planted inside an allowed
directory widens it. The resolved path is also the one the check opens, so what
was matched is what gets read — which means `${file}` in your syntax may spell
the name differently than the caller did.

!!! tip "Bookmarks and the first restricted check"

    A [bookmark](checks.md) is keyed on the file name, so the first check after
    you switch a bookmarked `check_logfile` to `allowed` may see a new key — if
    the resolved path spells the name differently — and read the file from the
    start once. It settles after that run. Switching to `predefined` avoids it
    if you point the configured name at the same spelling the caller used.

On Windows, matching ignores case; on Linux it does not, because
`/var/log/App.log` and `/var/log/app.log` are two different files there.

## check_files, check_single_file and check_disk_write

```ini
[/settings/disk]
file access = allowed
allowed files = C:/logs, D:/data/incoming/*.csv

; or, tighter:
file access = predefined

[/settings/disk/files]
logs = C:/logs
spool = D:/data/incoming
```

`[/settings/disk]` covers all three at once: `check_files` and
`check_single_file` read, `check_disk_write` writes. Entry shapes and path
resolution are exactly as for `check_logfile` above — a directory covers its
whole subtree, a wildcard is matched against the resolved path, and anything
else is a single file.

!!! info "These checks do not return file contents"

    There is no `line` or `content` keyword here. What they expose is the
    filesystem's *shape* — `path`, `filename`, `size`, `type`, `version` and the
    timestamps — for every file under the tree you point them at. Two keyword
    families go further without returning contents:

    * the checksum keywords (`md5_checksum`, `sha1_checksum`, `sha256_checksum`,
      `sha384_checksum`, `sha512_checksum`) hash the file, which confirms known
      content and, for a short or predictable file, effectively recovers it;
    * `line_count` and `version` are weaker oracles of the same kind.

    So the exposure is narrower than `check_logfile`'s but reaches much wider —
    whole trees rather than one named file — which is why it is worth setting
    even where you left `check_logfile` open.

While the disk checks are restricted, `check_files` also requires `pattern` to
be a plain file mask: it may not contain a path separator or `..`. The pattern
is appended to the directory being walked, so one carrying a path would
enumerate a tree the allow list never approved, no matter which root passed.

For `check_files` only the **scan root** is checked, not each file the walk
finds. That is sound rather than a shortcut: the recursion refuses to follow
symbolic links — directory links and junctions, and file links too — so
everything it yields is genuinely beneath a root which passed. It also keeps
the inner loop free of policy work on a check that may visit many thousands of
files.

`check_disk_write` cannot overwrite anything — the test file is created
exclusively, capped at 1 MB and deleted afterwards — but where you have narrowed
which paths a caller may name, that applies to writing them too.

## check_wmi

```ini
[/settings/wmi]
query access = allowed
allowed classes = Win32_Service, Win32_OperatingSystem, Win32_PerfFormattedData_*
allowed namespaces = root\cimv2
```

In `allowed` mode the query is parsed to find the class it reads, and that class
is held against `allowed classes`.

!!! warning "Only a plain SELECT can be checked by class"

    The gate refuses anything whose class it cannot determine with certainty,
    rather than guessing: `ASSOCIATORS OF`, `REFERENCES OF`, a class path
    carrying a namespace or a machine name (`root\cimv2:Win32_Process`), or more
    than one statement. Approving the wrong class would be worse than refusing,
    because the query would then read something the allow list never permitted.

    Queries like that are still perfectly usable — configure them as predefined
    queries, where you have already vouched for them:

    ```ini
    [/settings/wmi]
    query access = predefined

    [/settings/wmi/queries]
    disk-partitions = ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='C:'} WHERE AssocClass = Win32_LogicalDiskToPartition
    services = SELECT Name, State FROM Win32_Service
    ```

Two further restrictions apply whenever `query access` is not `any`:

* **Namespaces.** `allowed namespaces` lists the namespaces a caller may bind
  to. Leaving it empty means the namespace cannot be moved off the default
  `root\cimv2` at all — restricting the class while leaving the namespace open
  would let the same class name be read from a different provider.
* **Targets.** `target=` must name a target defined in
  `[/settings/wmi/targets]`. An unknown target is otherwise taken as a bare host
  name, which would let a caller point the check at a machine of its own
  choosing and hand it the configured credentials.

The `nscp wmi` command line is not affected. It is a local administrative tool,
and reaching it over REST already requires the `legacy` or `console.exec`
permission, which is administrator-equivalent in its own right.

## check_pdh

```ini
[/settings/system/windows]
counter access = allowed
allowed counters = \Processor(*)\*, \Memory\*, \System\*
```

`check_pdh` needs no new predefined list: the counters you already configured in
`[/settings/system/windows/counters]` *are* the predefined set, so
`counter access = predefined` means "only the counters I configured".

```ini
[/settings/system/windows]
counter access = predefined

[/settings/system/windows/counters]
threads = \System\Threads
disk_queue_length = \PhysicalDisk($INSTANCE$)\% Disk Time
```

The monitoring server then runs `check_pdh counter=threads`.

Wildcard entries are matched literally apart from `*` and `?`, so the
backslashes and parentheses a counter path is full of mean exactly what they
look like — `\Processor(_Total)\*` matches the `_Total` instance and not
`\Processor_Total\…`.

Two things to know about the allow list:

* The pattern is matched against the counter path **as the caller wrote it**,
  before any index expansion. On a mixed estate, list both the localized and the
  English spellings.
* The `counter:<alias>=<path>` form is gated exactly like `counter=`.

## check_registry_key and check_registry_value

```ini
[/settings/system/windows]
registry access = allowed
allowed registry keys = HKLM\SOFTWARE\MyApp, HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion

; or, tighter:
registry access = predefined

[/settings/system/windows/registry]
myapp = HKLM\SOFTWARE\MyApp
winver = HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion
```

!!! warning "This is the widest read of the lot"

    `check_registry_value` returns the value data itself — `string_value`
    renders `REG_SZ` expanded and `REG_BINARY` as hex — and `string_value` is
    part of the **default** detail syntax, so a caller needs no syntax argument
    to get it. With `value=*` and `recursive=true` at unlimited depth, one call
    walks an entire subtree. The registry is where Windows and third-party
    software keep autologon passwords, product keys, community strings and
    stored connection settings, so on a host where callers pass arguments this
    is the setting to reach for first.

Entries are **hierarchical, not glob**: an entry allows that key and everything
below it. The match is on whole key names, so `HKLM\SOFTWARE\MyApp` covers
`HKLM\SOFTWARE\MyApp\Settings` but not `HKLM\SOFTWARE\MyAppOther`. An entry
containing `*` or `?` is matched as a wildcard against the whole key instead,
for the cases where that is what you want.

Both hive spellings mean the same hive on either side of the comparison, so an
allow list written with `HKLM` still matches a caller who sent
`HKEY_LOCAL_MACHINE`. Matching ignores case, as the registry does.

Only the **starting** key is checked. Enumeration below it stays inside the
subtree by construction, so a key that passed cannot walk out of the allowed
part of the hive.

While `registry access` is not `any`, the `computer=` argument is refused
outright and only the local registry is reachable. A remote computer is a
destination the allow list says nothing about, and connecting to one
authenticates outbound as the machine account.

## check_eventlog

```ini
[/settings/eventlog]
log access = allowed
allowed logs = Application, System, Microsoft-Windows-Sysmon

; or, tighter:
log access = predefined

[/settings/eventlog/logs]
app = Application
sysmon = Microsoft-Windows-Sysmon/Operational
```

The event text comes back through the `message`, `strings` and `xml` keywords,
and `message` is part of the default detail syntax — so, as with the registry, a
caller gets content without asking for it. What that reaches is every channel
the agent can read: `Security`, the PowerShell and Sysmon operational channels,
and the task scheduler's, among others.

Entries are hierarchical on `/`, so `Microsoft-Windows-Sysmon` covers
`Microsoft-Windows-Sysmon/Operational` without naming each channel — and, by the
same whole-segment rule as the registry, does not cover
`Microsoft-Windows-SysmonOther`. A `*` or `?` in an entry switches it to
wildcard matching.

!!! note "The default channels are not exempt"

    With no `file=` argument `check_eventlog` reads `Application` and `System`.
    Those go through the gate too: leaving the argument off is not a way to opt
    them in, so list them if you want them.

Only channel names can be named — `check_eventlog` cannot be pointed at an
`.evtx` file on disk, because the query is always opened against a channel path.

## Choosing an approach

An access mode is not the only way to stop a caller reading more than you meant
it to, and it is not always the right one. There are four practical approaches,
and they trade the same thing against each other: how much the monitoring server
may decide for itself, versus how much you have to write down in the agent's
configuration.

| Approach | Where you set it | The monitoring server can still | What it costs you | Where it falls short |
|---|---|---|---|---|
| **Refuse arguments entirely** | `allow arguments = false` in `[/settings/NRPE/server]` (the default) for NRPE; the [`restricted` web role](../setup/securing.md#adding-a-dedicated-user) (`queries.execute.noargs`) for REST | Run the commands and aliases you defined, with the thresholds you baked into them | Every variation becomes a configuration entry — thresholds and output syntax included, not just the file or key | **Set per transport.** Each door has its own switch, so one you forget — or one added later — is not covered. It is also all-or-nothing across every check at once |
| **Allow a folder or subtree** | `access mode = allowed` with a directory, key or channel-family entry | Pick anything at or below what you allowed, and set its own thresholds and syntax | One setting per module, plus knowing what else lives under that area | Whatever lands there **later** is readable too. A folder shared with other applications is broader than it looks |
| **Allow specific items** | `access mode = allowed` with exact entries | Set thresholds, syntax and bookmarks — everything except choosing a different target | An agent configuration edit for each new file, key or channel | The paths live in the monitoring server's command line, so they are visible there and have to be kept in step with the agent |
| **Only predefined names** | `access mode = predefined` plus the module's names section | Run any name you defined, with its own thresholds and syntax | The most configuration up front, and the monitoring server's commands have to be rewritten to use names | No ad-hoc troubleshooting through the agent: a new target needs a configuration change and a reload |

### How they combine

They are not alternatives so much as layers, and the asymmetry is in the last
column above: **refusing arguments is set per transport, while an access mode is
set per check.**

Both doors can refuse arguments. NRPE has `allow arguments = false`, and the
web server has the built-in
[`restricted` role](../setup/securing.md#adding-a-dedicated-user), whose
`queries.execute.noargs` privilege runs checks and aliases but rejects any
request carrying a parameter. What you have to remember is that they are two
separate settings: closing one says nothing about the other, a user given
`monitoring` rather than `restricted` is unaffected by the NRPE switch, and a
transport added later starts open again. An access mode is written once on the
check and holds for every caller that reaches it.

So on a host where every door already refuses arguments — NRPE with
`allow arguments = false` and every web user on `restricted` — the access modes
add little you do not already have, and leaving them at `any` is a defensible
choice. On a host where any caller can pass arguments (a `monitoring` web user,
or NRPE with `allow arguments = true`), the access mode is the only one of the
four that applies.

They are worth having together where it matters: arguments off stops the caller
choosing anything, and the access mode means that if someone later turns
arguments on — or adds a `monitoring` user for an afternoon of troubleshooting
— the agent does not quietly become readable.

### Rolling it out

Because a configured name resolves in **every** mode, you do not have to pick
between the last two rows in one step:

1. Add the names first, under `[/settings/logfile/files]`,
   `[/settings/system/windows/registry]` and so on, and leave the mode at `any`.
2. Move the monitoring server onto those names. Nothing has been restricted yet,
   so a mistake shows up as a normal check failure rather than an outage.
3. Once the checks are green, set the mode to `predefined`.

If you would rather not rewrite the monitoring server's commands at all, stop at
the second row instead and allow the folders you already use.

### If you only change one thing

Set `registry access`. Of all the defaults it is the one that reaches furthest —
value data, rendered as hex for binary values, in the default syntax, over a
whole subtree at once.

### A typo will not open the gate

A mode that does not parse is refused rather than ignored: the check fails with
`expected any, allowed or predefined` and the module logs the error at startup.
A setting that decides what may be read must not fail open because it was
misspelled.

### Restricting which checks run at all

All of the above restricts what a check may *read*. It is worth saying that the
other half of the question — which commands a caller may run in the first place
— is a separate control, and often the cheaper win: see
[Permissions](permissions.md) for the policy engine, and the
[securing guide](../setup/securing.md#locking-down-which-commands-the-web-user-can-run)
for restricting a web user to a fixed list of commands. An access mode and a
permission policy answer different questions, and a tight deployment usually
wants both.

## What a refusal looks like

The check returns `UNKNOWN` with a message naming what was rejected and the
section that governs it:

```
Refusing file 'C:\Windows\System32\config\SAM': it is not in 'allowed files'
(see [/settings/logfile] in the configuration)
```

The contents of the allow list are deliberately left out — the caller has no
business enumerating it. When a check you expected to work is refused, the
agent log is where to look.

## See also

* [Securing NSClient++](../setup/securing.md) — where this fits among the other
  controls, and what to do first.
* [Checks In Depth](checks.md) — how filters, keywords and thresholds work.
* [Permissions](permissions.md) — restricting *which* checks a caller may run at
  all, which is the other half of the same question.
