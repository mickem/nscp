# SimpleCache

Stores status updates and allows for active checks to retrieve them

## Enable module

To enable this module and and allow using the commands you need to ass `SimpleCache = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
SimpleCache = enabled
```

## Queries

A quick reference for all available queries (check commands) in the SimpleCache module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                   |
|-----------------------------|-------------------------------|
| [check_cache](#check_cache) | Fetch results from the cache. |
| [list_cache](#list_cache)   | List all keys in the cache.   |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command    | Description                     |
|------------|---------------------------------|
| checkcache | Alias for: :query:`check_cache` |

### check_cache

Fetch results from the cache.

#### About `check_cache`

`check_cache` looks up a previously submitted result in the SimpleCache module's
in-memory cache and returns it **verbatim** — the same status, message and
performance data that were originally submitted.

This makes it different from every other check in NSClient++: it does not
measure anything. It is the read side of a store-and-forward arrangement, where
some other agent, script or scheduled check submits results into the `CACHE`
channel and a monitoring server later polls for them. That is how you get
passive results into a system that only knows how to poll, and how a host behind
a firewall can push results that the poller pulls from a reachable relay.

Because the cached response is returned as-is, the filter and syntax options
other checks take do not apply here.

##### Addressing an entry

Entries are stored under a key built from the `primary index` expression
configured on the module, which defaults to `${alias-or-command}` and can
combine `${command}`, `${host}`, `${channel}`, `${alias}` and
`${alias-or-command}`.

The setting's own description also lists `${message}` and `${result}`, but
**neither is implemented**: they fall through to the parser's error branch,
which logs `Invalid index: message` and contributes nothing to the key. An index
expression using them silently produces a shorter key than intended, so every
lookup that expects them misses. Check the agent log after changing
`primary index`, and confirm the result with
[`list_cache`](#list_cache).

You can either name the key outright with `key=` — which is used as given, not
parsed — or let the check assemble it from the same parts the writer used, by
passing `host=`, `command=`, `channel=` and `alias=`. Mixing the two is not
possible: an explicit `key=` wins and the individual parts are ignored.

##### When nothing is cached

`not-found-msg=` (default `Entry not found`) and `not-found-code=` (default
`unknown`) decide what a miss looks like. Leaving the default UNKNOWN is usually
right — it is distinguishable from a genuine OK, so a monitoring server can tell
"nobody has reported" apart from "reported healthy".

The cache is **held in memory only**. It does not survive an agent restart, and
a freshly started agent answers every lookup with the not-found result until
submissions start arriving again. Size the submitting side's interval so a
restart window does not read as a fleet of failures.

Use [`list_cache`](#list_cache) to see which keys are actually present, which is
the fastest way to debug a key expression that does not match.

The legacy alias `CheckCache` is accepted for backwards compatibility.

**Jump to section:**

* [Sample Commands](#check_cache_samples)
* [Command-line Arguments](#check_cache_options)


<a id="check_cache_samples"></a>
#### Sample Commands

**Put something in the cache first:**

`check_cache` only reads; something has to submit a result on the `CACHE`
channel. Here `check_and_forward` runs a check and forwards its result:

```
check_and_forward command=check_ok "arguments=message=backup finished" channel=CACHE alias=nightly_backup
OK: Message submitted: CACHE
```

**Read it back by key:**

The submitted result comes back verbatim — the same status and message the
original check produced.

```
check_cache key=nightly_backup
OK: backup finished
```

**Or let the key be assembled from its parts:**

With the default `primary index` of `${alias-or-command}`, naming the command is
equivalent to naming the key.

```
check_cache command=nightly_backup
OK: backup finished
```

Mixing the two forms does not work: an explicit `key=` wins and `host=`,
`command=`, `channel=` and `alias=` are ignored.

**A miss:**

The defaults report UNKNOWN, which a monitoring server can distinguish from a
genuine OK — "nobody has reported" is not the same as "reported healthy".

```
check_cache key=nothing
UNKNOWN: Entry not found
```

**Change what a miss looks like:**

```
check_cache key=nothing "not-found-msg=No result submitted in this cycle" not-found-code=critical
CRITICAL: No result submitted in this cycle
```

**With no key at all it is a syntax error, not a miss:**

```
check_cache
UNKNOWN: No key specified	...
```

**The cache does not survive a restart:**

It is held in memory only, so a freshly started agent answers every lookup with
the not-found result until submissions start arriving again.

```
check_cache key=nightly_backup
UNKNOWN: Entry not found
```



<a id="check_cache_options"></a>
#### Command-line Arguments

<a id="check_cache_key"></a>
<a id="check_cache_host"></a>
<a id="check_cache_command"></a>
<a id="check_cache_channel"></a>
<a id="check_cache_alias"></a>

| Option                                        | Default Value   | Description                                             |
|-----------------------------------------------|-----------------|---------------------------------------------------------|
| key                                           |                 | The key (will not be parsed)                            |
| host                                          |                 | The host to look for (translates into the key)          |
| command                                       |                 | The command to look for (translates into the key)       |
| channel                                       |                 | The channel to look for (translates into the key)       |
| alias                                         |                 | The alias to look for (translates into the key)         |
| [not-found-msg](#check_cache_not-found-msg)   | Entry not found | The message to display when a message is not found      |
| [not-found-code](#check_cache_not-found-code) | unknown         | The return status to return when a message is not found |



<h5 id="check_cache_not-found-msg">not-found-msg:</h5>

The message to display when a message is not found

*Default Value:* `Entry not found`

<h5 id="check_cache_not-found-code">not-found-code:</h5>

The return status to return when a message is not found

*Default Value:* `unknown`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### list_cache

List all keys in the cache.

#### About `list_cache`

`list_cache` returns the keys currently held in the SimpleCache module's
in-memory cache, as a comma-separated list.

It is a debugging aid rather than a monitoring check. When
[`check_cache`](#check_cache) reports `Entry not found`, the usual cause is that
the key the reader assembles does not match the key the writer stored under —
different `primary index` expression, a host name that arrived in a different
form, an alias that was empty on submission. Listing the keys shows what is
actually there, and the mismatch is normally obvious at a glance.

It takes no options and reports every key, so on a busy relay the output can be
long.

Note that the returned **status is always UNKNOWN**, including on a successful
listing: the command reports data rather than a verdict, and has no notion of a
healthy key set to compare against. Read the message, not the status, and do not
wire this command up as an alerting check.

**Jump to section:**

* [Sample Commands](#list_cache_samples)
* [Command-line Arguments](#list_cache_options)


<a id="list_cache_samples"></a>
#### Sample Commands

**List the keys currently held:**

```
list_cache
UNKNOWN: nightly_backup
```

Note the status: **`list_cache` always returns UNKNOWN**, including on a
successful listing. Read the message, not the status, and do not wire this
command up as an alerting check.

**An empty cache:**

```
list_cache
UNKNOWN:
```

**Debugging a `check_cache` miss:**

This is what the command is for. When a lookup reports `Entry not found`, list
the keys and compare them with the one the reader is assembling — a mismatched
`primary index` expression, a host name in a different form, or an empty alias
on submission is usually obvious at a glance.

```
check_cache command=nightly_backup
UNKNOWN: Entry not found

list_cache
UNKNOWN: srv01-nightly_backup
```

Here the writer's `primary index` includes `${host}` while the reader asked for
the command name alone.



<a id="list_cache_options"></a>
#### Command-line Arguments

This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section            | Description |
|---------------------------|-------------|
| [/settings/cache](#cache) | CACHE       |


### CACHE <a id="/settings/cache"></a>

Section for simple cache module (SimpleCache.dll).

| Key                                   | Default Value       | Description         |
|---------------------------------------|---------------------|---------------------|
| [channel](#channel)                   | CACHE               | CHANNEL             |
| [primary index](#primary-cache-index) | ${alias-or-command} | PRIMARY CACHE INDEX |


```ini
# Section for simple cache module (SimpleCache.dll).
[/settings/cache]
channel=CACHE
primary index=${alias-or-command}
```

#### CHANNEL <a id="/settings/cache/channel"></a>

The channel to listen to.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cache](#/settings/cache) |
| Key:           | channel                             |
| Default value: | `CACHE`                             |


**Sample:**

```
[/settings/cache]
# CHANNEL
channel=CACHE
```

#### PRIMARY CACHE INDEX <a id="/settings/cache/primary index"></a>

Set this to the value you want to use as unique key for the cache.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the receiving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherwise command, ${message} = the message data (no escape), ${result} = The result status (number).


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cache](#/settings/cache) |
| Key:           | primary index                       |
| Default value: | `${alias-or-command}`               |


**Sample:**

```
[/settings/cache]
# PRIMARY CACHE INDEX
primary index=${alias-or-command}
```
