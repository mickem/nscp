---
icon: "🏷️"
modules: [filters, CheckSystem, CheckSystemUnix, CheckNet, CheckSecurity, CheckDocker, CheckDisk]
action: conditional
---
**Check-specific filter keywords that clashed with the generic summary
keywords are renamed.** A handful of checks registered their own keyword
named `status`, `count` or `total` — the same names as the built-in summary
keywords (`%(status)`, `%(count)`, …). The check-specific value won in
`filter`/`warning`/`critical` and `detail-syntax`, while `top-syntax` and
the reference documentation showed the generic one, which was confusing.
Each such keyword now has a distinct, documented name:

| Check | Old | New |
|-------|-----|-----|
| `check_cpu`, `check_cpu_utilization` | `total` | `usage` |
| `check_battery` | `status` | `battery_status` |
| `check_network` | `status`, `total` | `link_status`, `throughput` |
| `check_os_updates` | `count` | `updates` |
| `check_patch_age` | `count` | `patches` |
| `check_pending_reboot` | `count` | `signals` |
| `check_printjobs` | `status` | `job_status` |
| `check_printqueue` | `status` | `printer_status` |
| `check_installed_software` (Linux) | `status` | `package_status` |
| `check_activation` | `status` | `activation_status` |
| `check_docker` | `status` | `container_status` |
| `check_connections` | `count`, `total` | `connections`, `total_connections` |
| `check_dns` | `count` | `records` |
| `check_http` | `status` | `status_message` |
| `check_shadowcopy` | `count` | `copies` |
| `check_disk_health` | `total` | `size` |

**Existing configurations keep working:** the old names remain as
undocumented deprecated aliases with unchanged behaviour, so filters like
`check_cpu "warn=total > 80"` still work. Migrate to the new names at your
convenience; only they appear in the reference documentation. A few
behaviour changes ride along:

- `check_os_updates`' default output now reports the actual number of
  updates (rendered from the detail line via `${list}`) instead of the
  matched-row count the old default showed.
- Three default perfdata keys change because the default `perf-config`
  lists the renamed keyword: `check_cpu_utilization` (Linux)
  `cpu_total` → `cpu_usage`, `check_patch_age` `patch_count` →
  `patch_patches`, `check_pending_reboot` `reboot_count` →
  `reboot_signals`. Pass your own `perf-config=extra(...)` with the old
  (alias) keyword to keep the old key. All other perfdata keys are
  unchanged (the renames keep their original perf suffixes, and
  `check_connections`' default perf-config intentionally still uses the
  alias for series continuity).
