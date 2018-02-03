# SimpleCache

Stores status updates and allows for active checks to retrieve them




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


* [Command-line Arguments](#check_cache_options)





<a name="check_cache_help"/>
<a name="check_cache_help-pb"/>
<a name="check_cache_show-default"/>
<a name="check_cache_help-short"/>
<a name="check_cache_key"/>
<a name="check_cache_host"/>
<a name="check_cache_command"/>
<a name="check_cache_channel"/>
<a name="check_cache_alias"/>
<a name="check_cache_options"/>
#### Command-line Arguments


| Option                                        | Default Value   | Description                                             |
|-----------------------------------------------|-----------------|---------------------------------------------------------|
| help                                          | N/A             | Show help screen (this screen)                          |
| help-pb                                       | N/A             | Show help screen as a protocol buffer payload           |
| show-default                                  | N/A             | Show default values for a given command                 |
| help-short                                    | N/A             | Show help screen (short format).                        |
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


### list_cache

List all keys in the cache.


* [Command-line Arguments](#list_cache_options)





<a name="list_cache_help"/>
<a name="list_cache_help-pb"/>
<a name="list_cache_show-default"/>
<a name="list_cache_help-short"/>
<a name="list_cache_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |






## Configuration



| Path / Section            | Description |
|---------------------------|-------------|
| [/settings/cache](#cache) | CACHE       |



### CACHE <a id="/settings/cache"/>

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
| Used by:       | SimpleCache                         |


**Sample:**

```
[/settings/cache]
# CHANNEL
channel=CACHE
```



#### PRIMARY CACHE INDEX <a id="/settings/cache/primary index"></a>

Set this to the value you want to use as unique key for the cache.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} = The result status (number).





| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cache](#/settings/cache) |
| Key:           | primary index                       |
| Default value: | `${alias-or-command}`               |
| Used by:       | SimpleCache                         |


**Sample:**

```
[/settings/cache]
# PRIMARY CACHE INDEX
primary index=${alias-or-command}
```


