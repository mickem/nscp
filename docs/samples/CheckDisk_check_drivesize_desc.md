#### Optional mounts (`ignore-missing`)

By default a drive named with `drive=` that does not exist fails the whole
check:

```
check_drivesize drive=/data
Drive /data was not found
```

That is right when the mount is supposed to be there, and wrong when it is not
— a removable volume, a filesystem mounted only on some hosts of a group, a
share that is attached on demand. `ignore-missing=true` drops such drives
instead:

```
check_drivesize drive=/data ignore-missing=true
OK: No drives found
```

Drives that *do* exist are still checked normally, so mixing the two in one
call works and the missing one simply contributes nothing:

```
check_drivesize drive=/ drive=/data ignore-missing=true "warn=used > 90%" "crit=used > 95%"
OK All 1 drive(s) are ok|'/ used'=43.555GB;906.169;956.511;0;1006.854 '/ used %'=4%;90;95;0;100
```

**`ignore-missing=true` implies `empty-state=ok`.** Without that, a check whose
drives are *all* missing would report UNKNOWN — trading a false CRITICAL for a
false UNKNOWN, which still pages someone. Only the default is changed, so
asking for something else explicitly still wins:

```
check_drivesize drive=/data ignore-missing=true empty-state=warning
WARNING: No drives found
```

**On Windows, `require=` is unaffected.** Listing a drive there is an explicit
assertion that it is present, so it stays CRITICAL when absent even under
`ignore-missing` — which is the whole point of listing it. Use `drive=` +
`ignore-missing` for optional volumes and `require=` for mandatory ones; they
compose in one call.

The same option exists on [`check_files`](#check_files) (for scan paths) and
[`check_single_file`](#check_single_file) (for the file itself).
