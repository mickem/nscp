# CommandClient

A command line client, generally not used except with "nscp test".

## Enable module

To enable this module and and allow using the commands you need to ass `CommandClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CommandClient = enabled
```


## Configuration

| Path / Section        | Description |
|-----------------------|-------------|
| [/settings/cli](#cli) | CLI         |


### CLI <a id="/settings/cli"></a>

Section for the interactive command line client (nscp test).

| Key                           | Default Value | Description  |
|-------------------------------|---------------|--------------|
| [color](#color)               | true          | COLOR        |
| [history file](#history-file) |               | HISTORY FILE |
| [history size](#history-size) | 500           | HISTORY SIZE |


```ini
# Section for the interactive command line client (nscp test).
[/settings/cli]
color=true
history size=500
```

#### COLOR <a id="/settings/cli/color"></a>

Colour the prompt, syntax highlight what is typed and colour log messages by severity. Turn this off for a terminal that renders the colours badly.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cli](#/settings/cli)     |
| Key:           | color                               |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `true`                              |


**Sample:**

```
[/settings/cli]
# COLOR
color=true
```

#### HISTORY FILE <a id="/settings/cli/history file"></a>

Where to keep the interactive prompt's command history. Empty means the per-user default: %APPDATA%\\NSClient++\\console-history.txt on Windows, $XDG_STATE_HOME/nscp/console-history or ~/.nscp_history on other platforms. History is only read and written when the prompt is attached to a terminal.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cli](#/settings/cli)     |
| Key:           | history file                        |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/cli]
# HISTORY FILE
history file=
```

#### HISTORY SIZE <a id="/settings/cli/history size"></a>

How many commands to keep in the history file. Set to 0 to turn persistent history off entirely - nothing is then read from or written to disk, which is what you want if commands typed at the prompt may carry credentials.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cli](#/settings/cli)     |
| Key:           | history size                        |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `500`                               |


**Sample:**

```
[/settings/cli]
# HISTORY SIZE
history size=500
```
