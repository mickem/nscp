# Test mode: the interactive console

`nscp test` runs the agent in the foreground with the same configuration the
service uses, logs to the console at debug level, and gives you a prompt to
type commands at. It is the first thing to reach for when a check misbehaves:
you see the log and the check result side by side, without a monitoring server
in the middle.

```
nscp test
```

Everything the service does still happens — modules load, listeners bind,
schedules fire — so stop the service first if you are testing something that
wants a port:

=== "Windows"

    ```
    net stop nscp
    nscp test
    ... reproduce the problem ...
    exit
    net start nscp
    ```

=== "Linux"

    ```
    sudo systemctl stop nscp
    nscp test
    ... reproduce the problem ...
    exit
    sudo systemctl start nscp
    ```

## The prompt

Anything you type that is not one of the built-in verbs below is run as a
query, with arguments in the same `key=value` form the REST API and NRPE use:

```
nscp> check_drive drive=c: warning='free < 20%'
OK: All 1 drive(s) are ok
 Performance data: 'c: free'=181GB;44;22;0;223
```

The prompt is a full line editor:

| Key                          | What it does |
|------------------------------|--------------|
| `Tab`                        | Complete the word under the cursor |
| `Up` / `Down`                | Walk the command history |
| `Ctrl+R`                     | Search backwards through the history |
| `Left` / `Right`, `Home` / `End` | Move within the line |
| `Ctrl+W` / `Ctrl+U`          | Delete the previous word / everything before the cursor |
| `Ctrl+L`                     | Clear the screen |
| `Ctrl+D`, `Ctrl+C`           | Leave the prompt (same as `exit`) |

The rest of the usual readline bindings work too (`Ctrl+A`/`Ctrl+E`,
`Ctrl+K`, `Ctrl+Y`, `Alt+B`/`Alt+F`, …).

Completion knows what makes sense in each position: the built-in verbs and
every registered query at the start of a line, query names after `desc`, and
the query's own parameter names (offered as `name=`) once you are typing
arguments. The module verbs go by state rather than offering everything —
`load` and `enable` offer the modules that are *not* already loaded or enabled,
`unload` and `disable` the ones that are.

Names match on what you typed anywhere in them, not only at the front, so you
do not have to remember which half of a name comes first:

```
nscp> load syst⇥
nscp> load CheckSystem
```

The prefix still wins where it matches: `load check⇥` offers the modules that
*start* with it, and only when nothing starts with what you typed does the
match widen to the middle of the name. Case does not matter either way, and the
completion corrects it — `load checkd⇥` gives you `CheckDisk`.

The first `load`/`enable` completion in a session pauses for a moment. To know
what is available but not loaded, the agent has to look in the module directory
and open each module it finds there; it does that once and remembers the
answer, so only the first tab pays for it. Until it has, an unloaded module
name is left uncoloured rather than marked wrong.

What you type is coloured as you type it. The colour that matters is the one
for a name that does not resolve — a query the agent has not registered, or a
module it cannot find, shows up in red before you press `Enter`, which is
usually a typo or a module you forgot to enable.

That extends inside a filter. The value of `filter`, `warning`, `warn`,
`critical`, `crit` and `ok` is read as the expression it is, and the value of
`top-syntax`, `detail-syntax`, `ok-syntax`, `empty-syntax` and `perf-syntax` as
the template it is — keywords, filter functions, operators, numbers and string
literals each get their own colour, and a keyword the check does not offer goes
red:

```
nscp> check_drive "warning=fre < 20%"
                            ↑ red: check_drive has free, not fre
```

This is the error that otherwise costs the most time, because nothing goes
wrong: an expression naming a keyword that does not exist parses, matches
nothing, and the check comes back a confident `OK`. The same colouring catches
`size > 100GB`, where the unit has to be a single letter (`100G`), and a
misspelt placeholder in `detail-syntax=${fre}`.

The keywords come from the check itself — the list `keywords <query>` prints —
so they are always the ones that query actually offers, an alias's target
included. They are looked up the first time you type an `=` after one of those
options and remembered from then on, so only that one keystroke pays for it.
Until then, and for a check that is not filter based, names are left uncoloured
rather than marked wrong.

Log messages arriving while you are mid-command are printed above the prompt
and the line you were typing is redrawn underneath, so a busy agent does not
cost you the command you were halfway through.

## Built-in commands

| Command                            | What it does |
|------------------------------------|--------------|
| `help`                             | Show this list |
| `exit`                             | Leave the prompt and stop the agent |
| `queries`                          | List every registered query |
| `aliases`                          | List every query alias |
| `list`                             | List queries and aliases |
| `plugins [--all\|--loaded\|--unloaded]` | List modules and whether each is loaded |
| `modules [--all\|--loaded\|--unloaded]` | Same thing (alias for `plugins`) |
| `desc <query>`                     | Describe a query and its parameters |
| `keywords <query>`                 | List the filter keywords a query offers |
| `metrics [prefix]`                 | Show the metrics collected so far |
| `facts [path]`                     | Show the host inventory, or the subtree at a dotted path |
| `facts refresh`                    | Collect the inventory now, then show it |
| `settings`                         | Dump the effective settings |
| `exec <target> <command> [args]`   | Run a command on one specific module |
| `load <module>`                    | Load a module now, without changing the configuration |
| `unload <module>`                  | Unload a module now |
| `enable <module>`                  | Enable a module in the configuration and save |
| `disable <module>`                 | Disable a module in the configuration and save |
| `reload`                           | Reload every module |

`load` and `unload` are the fast loop when you are working out which module
provides a check: they take effect immediately and are forgotten on exit, while
`enable`/`disable` write to the configuration and survive a restart.

`plugins` on its own lists what is running, which is what you usually want:

```
nscp> plugins
[X]  CheckSystem    Various system related checks, such as CPU load, ...
[X]  CommandClient  A command line client, generally not used except with "nscp test".
```

A module or check command that is still young carries an `(experimental)`
marker after its name — in `plugins`, in `queries`/`aliases`/`list`, and as a
`Status:` line in `desc`:

```
nscp> queries
check_cpu                       Check that the load of the CPU(s) are within bounds.
check_temperature (experimental)  Check ACPI thermal zone temperatures.
```

It means the check works and is meant to be used, but its options, filter
keywords and output may change in a coming release — so pin what you depend on
and expect to revisit it after an upgrade.

`--unloaded` is the other half — the modules sitting in the module directory
that nothing has loaded — and `--all` is both in one list, sorted by name.
`--loaded` spells out the default. The first `--all` or `--unloaded` pauses:
to know what is there but not loaded, the agent has to read every module in the
directory. It reads their metadata only — nothing is started — and it remembers
the answer, so only the first one pays for it. This is the same scan the first
`load`/`enable` completion does, and either one warms it for the other.

`facts` prints the host inventory the core keeps — the fact sets the loaded
modules are configured to produce, as an indented tree, with the revision a
fleet server polls on:

```
nscp> facts
Revision: 7  Collected: 2026-09-22T10:00:00Z
Enabled: os, storage

  os:
    family: windows
    name: Windows 11
    version: 10.0.26200
  storage:
    volumes:
      - C:
          fs: NTFS
          label: System
          size_bytes: 512110190592
      - D:
          size_bytes: 1024209543168
```

A record in a list leads with its `id` — the drive letter, interface name or
service name the rest of its fields describe.

Facts are opt-in, so on a fresh install this says `Enabled: (none ...)` and
`(no facts collected)`. Which sets exist is part of the producing module's
configuration, and so is turning one on:

```ini
[/settings/system/windows/facts]
os = true
```

Pass a dotted path to print one subtree — `facts os`, `facts storage.volumes` —
and `facts refresh` collects now instead of waiting for the next round (the
interval is `[/settings/facts] interval`, an hour by default). A set that is
enabled but failed to collect is listed under `Errors:` with the reason rather
than quietly missing from the tree.

## History

Commands are remembered across sessions in a per-user file — under
`%APPDATA%\NSClient++\` on Windows, and `$XDG_STATE_HOME/nscp/` or
`~/.nscp_history` on other platforms. On POSIX it is created mode `0600`; on
Windows it inherits the per-user permissions of your roaming profile.

Commands typed at the prompt can carry credentials (a `password=` argument to a
client module, say), and those land in that file like anything else. To keep
nothing on disk, turn persistence off:

```ini
[/settings/cli]
history size = 0
```

The same section holds `history file`, to put the file somewhere else, and
`color`, to turn the colouring off for a terminal that renders it badly. See
the [CommandClient reference](../reference/generic/CommandClient.md).

## Piping commands in

When stdin is not a terminal there is no prompt, no history and no colour:
commands are read line by line and the agent keeps logging to stdout as usual.
That makes test mode scriptable —

```
printf 'check_uptime\ncheck_drive drive=c:\nexit\n' | nscp test
```

— and it is also why running the agent in the foreground with stdin closed (in
a container, or under a supervisor) behaves the way you would expect: it reads
nothing, stays up, and shuts down on `SIGTERM`.

## Turning up the log

Test mode logs at debug by default. For the full firehose:

```
nscp test --log trace
```

Log level and the rest of the logging options are covered in
[Settings](settings.md); the file the *service* writes is described in
[File Layout](file-layout.md).
