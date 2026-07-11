Checks free space on a UNC path (`\\server\share`), optionally authenticating
with alternate credentials. This fills a gap `check_drivesize` cannot: it only
sees OS-mounted drives and cannot take an arbitrary UNC path or supply
credentials.

| Keyword                 | Description                                                                  |
|-------------------------|------------------------------------------------------------------------------|
| `path`                  | The UNC path being checked.                                                  |
| `size`                  | Total size of the share in bytes (perf).                                     |
| `free`                  | Free space on the share in bytes (perf).                                     |
| `used`                  | Used space in bytes (perf).                                                  |
| `user_free`             | Free space available to the querying user, honouring per-user quotas (perf). |
| `free_pct` / `used_pct` | Percentage free / used (perf).                                               |

Options: `path=` (repeatable), `user=`, `password=`. Defaults mirror
`check_drivesize` (`used_pct > 80` warning, `> 90` critical). On Windows the free
space comes from `GetDiskFreeSpaceEx`, with an optional `WNetAddConnection2` for
alternate credentials that is disconnected after the query. On non-Windows the
path must already be mounted (alternate-credential UNC access is Windows-only).
