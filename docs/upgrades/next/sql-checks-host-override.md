---
icon: "🔒"
modules: [CheckMSSQL, CheckMySQL]
action: conditional
---
**SQL checks no longer send the configured credentials to a server the request names.**
A `check_mssql*` request that changes `server`, `driver`, `connection-string`
or `encrypt`, or turns `trust-cert` on where `/settings/mssql` has it off, and
a `check_mysql*` request that changes `host` or `port`, or turns `tls` off
where it is configured on, is now refused unless it supplies its own `user=`
and `password=`. For CheckMSSQL this includes the default, credential-less
setup: it authenticates with Windows integrated authentication, as the account
the agent runs as. A `connection-string=` from the request is only accepted
with the opt-in. To keep the old behaviour, set the new
`allow host override = true` under `/settings/mssql` or `/settings/mysql`:

```ini
[/settings/mssql]
allow host override = true
```

Checks that use the configured server, or pass their own credentials, are
unaffected, as is CheckMySQL with no credentials configured. CheckMSSQL also
gains `trust server certificate` (default `true`, as before) and `encrypt`
settings; set `trust server certificate = false` once the instance has a
certificate the agent trusts. See the
[security notice](../security/notices.md#sql-checks-could-send-the-configured-login-or-the-machine-account-to-a-caller-chosen-server).
