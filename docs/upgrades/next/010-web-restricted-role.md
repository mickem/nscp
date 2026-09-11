---
icon: "🔧"
modules: [WEBServer]
action: none
---
**A new built-in WEB role, `restricted`, runs checks without arguments.** Nothing
to do on an existing install: the role is added to `[/settings/WEB/server/roles]`
but no user is assigned to it, and `client`, `monitoring` and any custom role
holding `queries.execute` keep passing arguments exactly as before. The new role
holds `queries.execute.noargs` instead of `queries.execute`, which is the REST
equivalent of the NRPE server's `allow arguments = false`: the caller may run the
checks the agent defines, but a request carrying any query-string parameter is
refused with `403 Arguments are not allowed for this user`. Neither grant implies
the other, so the role can never widen into the full privilege; `full` (`*`)
still confers both.

```ini
[/settings/WEB/server/users/monitor]
role = restricted

[/settings/check helpers/alias]
check_root_disk = check_drivesize drive=/ warning=free<10% critical=free<5%
```

Give a restricted caller the checks that need arguments as aliases, as above, so
the arguments live in your configuration. Note that *every* query parameter
counts as an argument, including a credential passed the legacy way as
`?password=` or `?TOKEN=`, so such a client must authenticate with a header.
