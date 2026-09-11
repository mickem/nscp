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

That is what those checks are for, and on a host where only the configuration
decides what runs, it is not a problem. It becomes one where the *caller*
chooses the argument — NRPE with `allow arguments = true`, or the REST API —
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
allowed files = C:/logs, C:/inetpub/logs/LogFiles/*.log

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

For `check_files` only the **scan root** is checked, not each file the walk
finds. That is sound rather than a shortcut: the recursion already refuses to
follow symbolic links and reparse points, so everything it yields is genuinely
beneath a root which passed. It also keeps the inner loop free of policy work on
a check that may visit many thousands of files.

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

## Choosing a mode

* **Leave it at `any`** where the agent only answers a monitoring server you
  control and `allow arguments` is off. The configuration decides everything,
  and nothing here adds to that.
* **Use `allowed`** where you want a family of checks — every log under one
  directory, every file under one tree, every counter of one object — without
  enumerating each one.
* **Use `predefined`** where the monitoring server should run your checks and
  nothing else. This is the one to aim for on a host exposed to NRPE with
  `allow arguments = true` or to the REST API.

A typo in a mode setting is refused rather than ignored: the check fails with
`expected any, allowed or predefined` and the module logs the error at startup.
A setting that decides what may be read must not fail open because it was
misspelled.

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
