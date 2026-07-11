#### About `check_defender`

`check_defender` reports Microsoft Defender's own health, read from
`MSFT_MpComputerStatus` (WMI namespace `root\Microsoft\Windows\Defender`).
Where `check_antivirus` reads Security Center's coarse "any AV registered /
enabled / up-to-date" bits for *whatever* product is installed, this exposes the
Defender-specific depth operators actually threshold on the very common
Defender-only estate: how old the definitions are, when the machine last
scanned, and whether real-time and tamper protection are on.

Keywords (a single aggregate row):

| Keyword             | Description                                                        |
|---------------------|-------------------------------------------------------------------|
| `enabled`           | 1 if Defender antivirus / service is enabled                      |
| `realtime_enabled`  | 1 if real-time protection is on                                   |
| `tamper_protection` | 1 if tamper protection is on                                      |
| `signature_age`     | Antivirus signature (definition) age in **days** (-1 if unknown)  |
| `quick_scan_age`    | Days since the last quick scan (-1 if never / unknown)            |
| `full_scan_age`     | Days since the last full scan (-1 if never / unknown)             |
| `engine_version`    | Anti-malware engine version                                       |
| `signature_version` | Antivirus signature (definition) version                          |
| `product_version`   | Defender platform / product version                               |

Defaults: **WARNING** when `signature_age > 3`, **CRITICAL** when
`enabled = 0 or realtime_enabled = 0 or signature_age > 7`. A negative
(unknown / never) age never trips a threshold, so `full_scan_age` / `quick_scan_age`
are informational unless you add your own thresholds.

**Not-the-active-AV contract:** when a third-party antivirus has taken over,
Defender's status class is unavailable. The check then returns **UNKNOWN** with
an explanatory message (via the empty state) rather than a hard error — so it is
safe to deploy fleet-wide and only alerts where Defender is actually the
protecting product.
