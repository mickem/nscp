# SimpleCache

Stores status updates and allows for active checks to retrieve them



## List of commands

A list of all available queries (check commands)

| Command                     | Description                   |
|-----------------------------|-------------------------------|
| [check_cache](#check_cache) | Fetch results from the cache. |
| [list_cache](#list_cache)   | List all keys in the cache.   |


## List of command aliases

A list of all short hand aliases for queries (check commands)


| Command    | Description                     |
|------------|---------------------------------|
| checkcache | Alias for: :query:`check_cache` |


## List of Configuration


### Common Keys

| Path / Section                      | Key                                             | Description         |
|-------------------------------------|-------------------------------------------------|---------------------|
| [/settings/cache](#/settings/cache) | [channel](#/settings/cache_channel)             | CHANNEL             |
| [/settings/cache](#/settings/cache) | [primary index](#/settings/cache_primary index) | PRIMARY CACHE INDEX |





# Queries

A quick reference for all available queries (check commands) in the SimpleCache module.

## check_cache

Fetch results from the cache.


### Usage


| Option                                        | Default Value   | Description                                             |
|-----------------------------------------------|-----------------|---------------------------------------------------------|
| [help](#check_cache_help)                     | N/A             | Show help screen (this screen)                          |
| [help-pb](#check_cache_help-pb)               | N/A             | Show help screen as a protocol buffer payload           |
| [show-default](#check_cache_show-default)     | N/A             | Show default values for a given command                 |
| [help-short](#check_cache_help-short)         | N/A             | Show help screen (short format).                        |
| [key](#check_cache_key)                       |                 | The key (will not be parsed)                            |
| [host](#check_cache_host)                     |                 | The host to look for (translates into the key)          |
| [command](#check_cache_command)               |                 | The command to look for (translates into the key)       |
| [channel](#check_cache_channel)               |                 | The channel to look for (translates into the key)       |
| [alias](#check_cache_alias)                   |                 | The alias to look for (translates into the key)         |
| [not-found-msg](#check_cache_not-found-msg)   | Entry not found | The message to display when a message is not found      |
| [not-found-code](#check_cache_not-found-code) | unknown         | The return status to return when a message is not found |


<a name="check_cache_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_cache_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_cache_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_cache_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_cache_key"/>
### key



**Description:**
The key (will not be parsed)

<a name="check_cache_host"/>
### host



**Description:**
The host to look for (translates into the key)

<a name="check_cache_command"/>
### command



**Description:**
The command to look for (translates into the key)

<a name="check_cache_channel"/>
### channel



**Description:**
The channel to look for (translates into the key)

<a name="check_cache_alias"/>
### alias



**Description:**
The alias to look for (translates into the key)

<a name="check_cache_not-found-msg"/>
### not-found-msg


**Deafult Value:** Entry not found

**Description:**
The message to display when a message is not found

<a name="check_cache_not-found-code"/>
### not-found-code


**Deafult Value:** unknown

**Description:**
The return status to return when a message is not found

## list_cache

List all keys in the cache.


### Usage


| Option                                   | Default Value | Description                                   |
|------------------------------------------|---------------|-----------------------------------------------|
| [help](#list_cache_help)                 | N/A           | Show help screen (this screen)                |
| [help-pb](#list_cache_help-pb)           | N/A           | Show help screen as a protocol buffer payload |
| [show-default](#list_cache_show-default) | N/A           | Show default values for a given command       |
| [help-short](#list_cache_help-short)     | N/A           | Show help screen (short format).              |


<a name="list_cache_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="list_cache_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="list_cache_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="list_cache_help-short"/>
### help-short



**Description:**
Show help screen (short format).



# Configuration

<a name="/settings/cache"/>
## CACHE

Section for simple cache module (SimpleCache.dll).

```ini
# Section for simple cache module (SimpleCache.dll).
[/settings/cache]
channel=CACHE
primary index=${alias-or-command}

```


| Key                                             | Default Value       | Description         |
|-------------------------------------------------|---------------------|---------------------|
| [channel](#/settings/cache_channel)             | CACHE               | CHANNEL             |
| [primary index](#/settings/cache_primary index) | ${alias-or-command} | PRIMARY CACHE INDEX |




<a name="/settings/cache_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cache](#/settings/cache) |
| Key:           | channel                             |
| Default value: | `CACHE`                             |
| Used by:       | SimpleCache                         |


#### Sample

```
[/settings/cache]
# CHANNEL
channel=CACHE
```


<a name="/settings/cache_primary index"/>
### primary index

**PRIMARY CACHE INDEX**

Set this to the value you want to use as unique key for the cache.
Can be any arbitrary string as well as include any of the following special keywords:${command} = The command name, ${host} the host, ${channel} the recieving channel, ${alias} the alias for the command, ${alias-or-command} = alias if set otherweise command, ${message} = the message data (no escape), ${result} = The result status (number).




| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/cache](#/settings/cache) |
| Key:           | primary index                       |
| Default value: | `${alias-or-command}`               |
| Used by:       | SimpleCache                         |


#### Sample

```
[/settings/cache]
# PRIMARY CACHE INDEX
primary index=${alias-or-command}
```


