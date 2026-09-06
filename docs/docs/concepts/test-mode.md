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

The first `load`/`enable` completion in a session pauses for a moment. To know
what is available but not loaded, the agent has to look in the module directory
and open each module it finds there; it does that once and remembers the
answer, so only the first tab pays for it. Until it has, an unloaded module
name is left uncoloured rather than marked wrong.

What you type is coloured as you type it. The colour that matters is the one
for a name that does not resolve — a query the agent has not registered, or a
module it cannot find, shows up in red before you press `Enter`, which is
usually a typo or a module you forgot to enable.

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
| `plugins`                          | List every module and whether it is loaded |
| `desc <query>`                     | Describe a query and its parameters |
| `metrics [prefix]`                 | Show the metrics collected so far |
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
