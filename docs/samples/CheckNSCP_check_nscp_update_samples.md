**Default check (any newer release is CRITICAL):**

Both the default warning and the default critical threshold are
`update_available = 1`.

```
check_nscp_update
CRITICAL: 0.17.2 (latest: 0.18.1)|'version_update_available'=1;1;1
```

An agent on the current release:

```
check_nscp_update
OK: 0.18.1 (latest: 0.18.1)|'version_update_available'=0;1;1
```

**Something less noisy — only alert on falling more than one release behind:**

```
check_nscp_update "warn=versions_behind > 0" "crit=versions_behind > 1"
OK: 0.18.1 (latest: 0.18.1)|'version_versions_behind'=0;0;1
```

**Point an operator at the release:**

```
check_nscp_update "detail-syntax=${version} -> ${latest_version} (${tag}, published ${published}) ${url}"
CRITICAL: 0.17.2 -> 0.18.1 (0.18.1, published 2026-08-14) https://github.com/mickem/nscp/releases/tag/0.18.1
```

**When GitHub cannot be reached:**

The failure lands in `error`, but `update_available` stays 0 — so with the
default thresholds a blocked agent reports **exactly the same OK** as an
up-to-date one:

```
check_nscp_update "detail-syntax=avail=${update_available} latest=${latest_version} err=${error}"
OK: avail=0 latest= err=HTTP 426 from https://api.github.com/repos/mickem/nscp/releases/latest|'version_update_available'=0;1;1
```

Threshold on `error` explicitly if you rely on this check:

```
check_nscp_update "warn=update_available = 1" "crit=error != ''"
CRITICAL: 0.18.1 (latest: )|'version_update_available'=0;1;0
```

**Caching:**

The GitHub result is cached (24 hours by default) to stay inside the API's rate
limit, so the check is cheap to schedule often — but can lag a fresh release by
up to the cache lifetime.
