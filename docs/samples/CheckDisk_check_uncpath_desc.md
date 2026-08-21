Checks free space on a UNC path (`\\server\share`), optionally authenticating
with alternate credentials. This fills a gap `check_drivesize` cannot: it only
sees OS-mounted drives and cannot take an arbitrary UNC path or supply
credentials.

The space keywords (`size`, `free`, `used`, `user_free`, `free_pct`,
`used_pct`) are emitted as performance data when used in thresholds.

Options: `path=` (repeatable), `user=`, `password=`. Defaults mirror
`check_drivesize` (`used_pct > 80` warning, `> 90` critical). On Windows the free
space comes from `GetDiskFreeSpaceEx`, with an optional `WNetAddConnection2` for
alternate credentials that is disconnected after the query. On non-Windows the
path must already be mounted (alternate-credential UNC access is Windows-only).
