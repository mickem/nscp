**Check Storage Spaces pool health and capacity (Windows):**

```
check_storagepool
OK: All storage pools are healthy.
'Pool1 free_pct'=64%;20;10 'Pool1 capacity'=8.0T;; 'Pool1 used'=2.9T;;
```

By default the check is WARNING when a pool reports `Warning` health or drops
below 20% free, and CRITICAL when a pool is `Unhealthy` or below 10% free. A
system with no Storage Spaces pools returns OK.

**Alert only on pool health, ignoring capacity:**

```
check_storagepool "warn=health_status = 'Warning'" "crit=health_status = 'Unhealthy' or is_readonly = 1" "detail-syntax=${name}: ${health_status} (${operational_status})"
CRITICAL: Data: Unhealthy (Unhealthy)
```

Keywords: `name`, `health_status` (Healthy/Warning/Unhealthy/Unknown),
`operational_status`, `capacity`, `used`, `free`, `free_pct`, `used_pct`,
`is_readonly`.
