---
title: "The `legacy` WEB permission is flagged and no longer seeded by default"
fixed_in: 0.16.2
severity: "Medium"
reported_by: "[yagust](https://github.com/yagust)"
modules: [WEBServer]
action: none
---
The `legacy` WEB grant unlocks the deprecated `POST /query.pb` and
`GET /query/{name}` endpoints, which dispatch through the same command registry
as the versioned `/api/v2/queries` API — so a token holding the `legacy`
permission can run any registered check or command (including any configured
`CheckExternalScripts` command) even without `queries.execute`. This is
intended compatibility behaviour for clients that predate the versioned API,
but it was under-documented and more powerful than the role name suggests.

The built-in `legacy` role is no longer seeded on fresh installs; NSClient++
now logs a `SECURITY` warning at startup — and from `nscp web add-role` /
`add-user` — for any role whose grant includes the `legacy` token (so a custom
role such as `reporting = legacy,login.get` is flagged too); and the capability
is documented in the securing guide.

**What to do:** existing installs are unaffected (the role stays in their
config and keeps working). Only grant `legacy` to trusted legacy systems that
cannot use the versioned `/api/v2/queries` endpoints; see
[Securing NSClient++](../setup/securing.md).
