#### About `check_rds_licenses`

`check_rds_licenses` reads the Remote Desktop licensing key packs from WMI
(`Win32_TSLicenseKeyPack` in `root\cimv2`) and reports one record per key pack
with the issued versus available CAL counts. Run it on the RD **licensing**
server (the host holding the "Remote Desktop Licensing" role) — on any other
host the class does not exist and the check returns UNKNOWN with a clear
"role is not installed" message.

CAL exhaustion locks new users out of an RDS farm, so the default thresholds
alert on the available count: **warning** when `available < 10 and total > 0`,
**critical** when `available = 0 and total > 0`. The `total > 0` guard keeps
the built-in/unlimited key packs (which report no meaningful counts) from
tripping the thresholds.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                        |
|-------------------|--------------------------------------------------------------------|
| `description`     | License type and model, e.g. `RDS Per User CAL` (`TypeAndModel`)   |
| `product_version` | Product version the pack applies to, e.g. `Windows Server 2022`    |
| `type`            | Key pack type: `unknown`, `retail`, `volume`, `concurrent`, `temporary`, `open`, `built-in` |
| `keypack_type`    | The raw numeric `KeyPackType` value                                |
| `id`              | Key pack id                                                        |
| `total`           | Total licenses in the key pack                                     |
| `issued`          | Licenses issued to clients                                         |
| `available`       | Licenses still available                                           |

Per-user CALs are not enforced by the session host, so `issued` growing past
`total` is possible in per-user mode; alert on `available` (as the defaults
do) or on `issued` explicitly if you track compliance.
