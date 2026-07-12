#### About `check_shadowcopy`

`check_shadowcopy` verifies **Volume Shadow Copy Service (VSS)** snapshots — the
data behind "Previous Versions", scheduled restore points and many backup
products. It answers "does each volume still have a *recent* shadow copy, and is
shadow storage healthy?", which is often the first sign that a backup / snapshot
job has silently stopped running.

It reads `Win32_ShadowCopy` (one row per snapshot) and groups it by volume, then
joins per-volume usage from `Win32_ShadowStorage`. One row is produced per volume
that has at least one shadow copy.

Keywords:

| Keyword       | Description                                                                        |
|---------------|------------------------------------------------------------------------------------|
| `volume`      | The volume the shadow copies belong to (VolumeName device path)                    |
| `count`       | Number of shadow copies on this volume                                             |
| `newest`      | **Seconds** since the newest shadow copy (-1 if the date is unknown)               |
| `newest_date` | Timestamp of the newest shadow copy (UTC)                                          |
| `used`        | Shadow storage currently used on this volume, in bytes                             |
| `allocated`   | Shadow storage currently allocated, in bytes                                       |
| `max_size`    | Shadow storage maximum, in bytes (0 when unbounded / not resolved)                 |
| `used_pct`    | Percentage of the shadow-storage maximum in use (0 when `max_size` is unbounded)   |

`newest` is seconds, so threshold it with durations: `newest > 26h`, `newest > 2d`.

Defaults: **WARNING** when `newest > 26h`, **CRITICAL** when `newest > 50h` — i.e.
tuned for roughly **daily** snapshots; loosen them (`newest > 8d`) for weekly
schedules. The empty state is **OK**: a volume with no shadow copies is common
and not inherently a problem. If snapshots are *required*, pass
`empty-state=critical` so their absence is alerted.

**Caveats:** shadow copies are transient (VSS deletes the oldest when storage
fills), so a shrinking `count` or rising `used_pct` is an early warning that
older restore points are being aged out. `max_size` is 0 when shadow storage is
configured as "unbounded", which makes `used_pct` inert by design.
