# CauseCrashes

*DO NOT USE* This module is usefull except for debugging purpouses and outright dangerous as it allows someone remotley to crash your client!




## Queries

A quick reference for all available queries (check commands) in the CauseCrashes module.

**List of commands:**

A list of all available queries (check commands)

| Command                       | Description                                                                  |
|-------------------------------|------------------------------------------------------------------------------|
| [crash_client](#crash_client) | Raise a fatal exception (zero pointer reference) and cause NSClient++ crash. |


**List of command aliases:**

A list of all short hand aliases for queries (check commands)


| Command     | Description                      |
|-------------|----------------------------------|
| crashclient | Alias for: :query:`crash_client` |


### crash_client

Raise a fatal exception (zero pointer reference) and cause NSClient++ crash.

* [Samples](#crash_client_samples)

* [Command-line Arguments](#crash_client_options)


<a name="crash_client_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CauseCrashes_crash_client_samples.md)_

Configuration to setup the module::

```
[/modules]
NRPEServer = enabled
CauseCrashes = enabled

[/settings/NRPE/server]
allowed hosts = 127.0.0.1
```

Then execute the following command on Nagios::

```
nscp nrpe --host 127.0.0.1 --command crashclient
```

Then execute the following command on the NSClient++ machine::

```
nscp test
...
crashclient
```

This will cause NSClient++ to crash so please dont do this.


<a name="crash_client_help"/>
<a name="crash_client_help-pb"/>
<a name="crash_client_show-default"/>
<a name="crash_client_help-short"/>
<a name="crash_client_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                   |
|--------------|---------------|-----------------------------------------------|
| help         | N/A           | Show help screen (this screen)                |
| help-pb      | N/A           | Show help screen as a protocol buffer payload |
| show-default | N/A           | Show default values for a given command       |
| help-short   | N/A           | Show help screen (short format).              |






