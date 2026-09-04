#### About `check_nscp_update`

`check_nscp_update` compares the running build against the latest release
published on the NSClient++ GitHub releases page, so a fleet can alert on
"this agent is out of date" without an external inventory.

The comparison is exposed as filter keywords: `update_available` (0/1),
`versions_behind`, and the latest release's `latest_version` /
`latest_major` / `latest_minor` / `latest_release` / `latest_build` alongside
the running `version` / `major` / `minor` / `release` / `build`. `tag`,
`published` and `url` identify the release itself, so the alert can point an
operator straight at the download.

Both the default warning and the default critical threshold are
`update_available = 1`, so a bare call reports CRITICAL as soon as any newer
release exists. Most fleets want something less noisy — for example only caring
about falling more than one release behind:

```
check_nscp_update "warn=versions_behind > 0" "crit=versions_behind > 1"
```

**The result is cached** (24 hours by default) because the GitHub API is rate
limited per source IP, and a fleet of agents checking on every poll would
exhaust that budget quickly. The cache means the check is cheap to schedule
often, but also that it can lag a fresh release by up to the cache lifetime.

**This check makes an outbound HTTPS request to github.com.** On a host with no
egress — which is the normal case for a monitored server — it cannot work;
either allow that one destination or run the check from a single management host
rather than fleet-wide.

When the request fails, the reason lands in the `error` keyword — but the
**status stays OK**, because `update_available` is 0 and that is all the default
thresholds look at. An agent that cannot reach GitHub therefore reports exactly
the same thing as an agent that is up to date. If you rely on this check, say so
explicitly:

```
check_nscp_update "warn=update_available = 1" "crit=error != ''"
```
