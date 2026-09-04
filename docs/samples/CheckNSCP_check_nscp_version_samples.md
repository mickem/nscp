**Report the running version:**

```
check_nscp_version
OK: 0.18.1 (2026-08-14)
```

**Alert when an agent has fallen behind a version you standardised on:**

The parts are numeric, so they threshold directly. `version` is
`release.major.minor`, so 0.18.1 is release 0, major 18, minor 1.

```
check_nscp_version "crit=major < 12"
CRITICAL: 0.11.4 (2024-03-09)|'version_major'=11;0;12
```

An agent that is current stays OK:

```
check_nscp_version "crit=major < 12"
OK: 0.18.1 (2026-08-14)|'version_major'=18;0;12
```

**Inspect all the parts:**

```
check_nscp_version "detail-syntax=${version} major=${major} minor=${minor} release=${release} build=${build}"
OK: 0.18.1 major=18 minor=1 release=0 build=0
```

`build` is only meaningful on builds before 0.6.0; newer releases report `0`.

**As a heartbeat probe:**

There are no default thresholds, so a bare call is always OK — anything else
means the agent is not answering.

```
check_nrpe --host 192.168.56.103 --command check_nscp_version
OK: 0.18.1 (2026-08-14)
```
