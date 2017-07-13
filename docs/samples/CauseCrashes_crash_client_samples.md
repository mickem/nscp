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