#### About `check_defender`

`check_defender` reports Microsoft Defender's own health, read from
`MSFT_MpComputerStatus` (WMI namespace `root\Microsoft\Windows\Defender`).
Where `check_antivirus` reads Security Center's coarse "any AV registered /
enabled / up-to-date" bits for *whatever* product is installed, this exposes the
Defender-specific depth operators actually threshold on the very common
Defender-only estate: how old the definitions are, when the machine last
scanned, and whether real-time and tamper protection are on.

All keywords are reported on a single aggregate row.

Defaults: **WARNING** when `signature_age > 3`, **CRITICAL** when
`enabled = 0 or realtime_enabled = 0 or signature_age > 7`. A negative
(unknown / never) age never trips a threshold, so `full_scan_age` / `quick_scan_age`
are informational unless you add your own thresholds.

**Not-the-active-AV contract:** when a third-party antivirus has taken over,
Defender's status class is unavailable. The check then returns **UNKNOWN** with
an explanatory message (via the empty state) rather than a hard error — so it is
safe to deploy fleet-wide and only alerts where Defender is actually the
protecting product.
