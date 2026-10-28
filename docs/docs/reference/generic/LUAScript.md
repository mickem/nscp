# LUAScript

Loads and processes internal Lua scripts

## Enable module

To enable this module and allow using the commands you need to add `LUAScript = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
LUAScript = enabled
```


## Configuration

| Path / Section                        | Description    |
|---------------------------------------|----------------|
| [/settings/default](#default-values)  | Default values |
| [/settings/lua](#/settings/lua)       |                |
| [/settings/lua/scripts](#lua-scripts) | Lua scripts    |


### Default values <a id="/settings/default"></a>

Default values used in other config sections.

| Key                                                 | Default Value | Description                 |
|-----------------------------------------------------|---------------|-----------------------------|
| [allowed hosts](#allowed-hosts)                     | 127.0.0.1     | Allowed hosts               |
| [bind to](#bind-to-address)                         |               | BIND TO ADDRESS             |
| [cache allowed hosts](#cache-list-of-allowed-hosts) | true          | Cache list of allowed hosts |
| [encoding](#nrpe-payload-encoding)                  |               | NRPE PAYLOAD ENCODING       |
| [inbox](#inbox)                                     | inbox         | INBOX                       |
| [password](#password)                               |               | Password                    |
| [script root](#script-root)                         |               | Script root                 |
| [socket queue size](#listen-queue)                  | 0             | LISTEN QUEUE                |
| [thread pool](#thread-pool)                         | 10            | THREAD POOL                 |
| [timeout](#timeout)                                 | 30            | TIMEOUT                     |
| [timezone](#timezone)                               | local         | Timezone                    |


```ini
# Default values used in other config sections.
[/settings/default]
allowed hosts=127.0.0.1
cache allowed hosts=true
inbox=inbox
socket queue size=0
thread pool=10
timeout=30
timezone=local
```

#### Allowed hosts <a id="/settings/default/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | allowed hosts                           |
| Default value: | `127.0.0.1`                             |


**Sample:**

```
[/settings/default]
# Allowed hosts
allowed hosts=127.0.0.1
```

#### BIND TO ADDRESS <a id="/settings/default/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | bind to                                 |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```

#### Cache list of allowed hosts <a id="/settings/default/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | cache allowed hosts                     |
| Default value: | `true`                                  |


**Sample:**

```
[/settings/default]
# Cache list of allowed hosts
cache allowed hosts=true
```

#### NRPE PAYLOAD ENCODING <a id="/settings/default/encoding"></a>




| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | encoding                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```

#### INBOX <a id="/settings/default/inbox"></a>

The default channel to post incoming messages on


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | inbox                                   |
| Default value: | `inbox`                                 |


**Sample:**

```
[/settings/default]
# INBOX
inbox=inbox
```

#### Password <a id="/settings/default/password"></a>

Password an inbound caller has to present. Stored hashed (pbkdf2-sha256$...) when written by \`nscp web install\` or \`nscp web password --set\`; a clear-text value written by hand is still accepted, and is hashed in place when re-set. This is a password to verify against, not key material: NSCA encrypts with its shared secret instead of verifying it, so it keeps its own key under /settings/NSCA/server (or the NSCAClient default target) and never reads this one.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | password                                |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# Password
password=
```

=== "Windows"

    #### Script root <a id="/settings/default/script root"></a>

    The folder this module's scripts live in: ${scripts} by default (empty, or \`default\`). \`fleet\` is ${fleet-folder}/scripts, where the fleet sync stages a bundle's \`scripts/\` entries - a bundle that ships scripts sets this to \`fleet\` and refers to them as \`scripts\\<name>\`. \`shared\` is ${shared-path}/scripts; any other value is a folder (${...} tokens expanded). Python and Lua scripts live in its \`python\` and \`lua\` sub-folders. An external script is started in the folder *above* it, with a relative command such as \`scripts\\check.bat\` resolved against that folder - the rule the default layout has always followed, so a scripts folder moved elsewhere runs unchanged, scripts that call each other by relative path included. Set it under [/settings/default] to apply to every script module at once. SECURITY: whoever can write to this folder can change what the service runs. It is also what ext-scr show/delete are confined to and where ext-scr add --import copies to; ext-scr add --script <path> is not bounded by it (an administrator-only action, equivalent to editing the configuration directly).


    | Key            | Description                             |
    |----------------|-----------------------------------------|
    | Path:          | [/settings/default](#/settings/default) |
    | Key:           | script root                             |
    | Default value: | _N/A_                                   |


    **Sample:**

    ```
    [/settings/default]
    # Script root
    script root=
    ```

=== "Linux"

    #### Script root <a id="/settings/default/script root"></a>

    The folder this module's scripts live in: ${scripts} by default (empty, or \`default\`). \`fleet\` is ${fleet-folder}/scripts, where the fleet sync stages a bundle's \`scripts/\` entries - a bundle that ships scripts sets this to \`fleet\` and refers to them as \`scripts\\<name>\`. \`shared\` is ${shared-path}/scripts; any other value is a folder (${...} tokens expanded). Python and Lua scripts live in its \`python\` and \`lua\` sub-folders. An external script is started in the folder *above* it, with a relative command such as \`scripts\\check.bat\` resolved against that folder - the rule the default layout has always followed, so a scripts folder moved elsewhere runs unchanged, scripts that call each other by relative path included. Set it under [/settings/default] to apply to every script module at once. SECURITY: whoever can write to this folder can change what the service runs.


    | Key            | Description                             |
    |----------------|-----------------------------------------|
    | Path:          | [/settings/default](#/settings/default) |
    | Key:           | script root                             |
    | Default value: | _N/A_                                   |


    **Sample:**

    ```
    [/settings/default]
    # Script root
    script root=
    ```

#### LISTEN QUEUE <a id="/settings/default/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | socket queue size                       |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `0`                                     |


**Sample:**

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```

#### THREAD POOL <a id="/settings/default/thread pool"></a>




| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | thread pool                             |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `10`                                    |


**Sample:**

```
[/settings/default]
# THREAD POOL
thread pool=10
```

#### TIMEOUT <a id="/settings/default/timeout"></a>

Timeout (in seconds) when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timeout                                 |
| Default value: | `30`                                    |


**Sample:**

```
[/settings/default]
# TIMEOUT
timeout=30
```

#### Timezone <a id="/settings/default/timezone"></a>

Timezone used to render dates such as boot time. Accepts 'local' (default), 'utc', or any POSIX TZ string parseable by Boost.Date_time (e.g. 'MST-07' or 'EST-05EDT,M3.2.0,M11.1.0').


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timezone                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `local`                                 |


**Sample:**

```
[/settings/default]
# Timezone
timezone=local
```

### /settings/lua <a id="/settings/lua"></a>



| Key                         | Default Value | Description |
|-----------------------------|---------------|-------------|
| [script root](#script-root) |               | Script root |


```ini
# 
[/settings/lua]
```

#### Script root <a id="/settings/lua/script root"></a>

The folder this module's scripts live in: ${scripts} by default (empty, or \`default\`). \`fleet\` is ${fleet-folder}/scripts, where the fleet sync stages a bundle's \`scripts/\` entries - a bundle that ships scripts sets this to \`fleet\` and refers to them as \`scripts\\<name>\`. \`shared\` is ${shared-path}/scripts; any other value is a folder (${...} tokens expanded). Python and Lua scripts live in its \`python\` and \`lua\` sub-folders. An external script is started in the folder *above* it, with a relative command such as \`scripts\\check.bat\` resolved against that folder - the rule the default layout has always followed, so a scripts folder moved elsewhere runs unchanged, scripts that call each other by relative path included. Set it under [/settings/default] to apply to every script module at once. SECURITY: whoever can write to this folder can change what the service runs.


| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/lua](#/settings/lua) |
| Key:           | script root                     |
| Default value: | _N/A_                           |


**Sample:**

```
[/settings/lua]
# Script root
script root=
```

### Lua scripts <a id="/settings/lua/scripts"></a>

A list of scripts available to run from the LuaScript module.


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





