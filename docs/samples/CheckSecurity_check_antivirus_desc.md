#### About `check_antivirus`

`check_antivirus` reports the antivirus products registered with the **Windows
Security Center** (WMI `root\SecurityCenter2` `AntiVirusProduct`), including
third-party AV and Windows Defender. This is distinct from `check_os_updates`
(which counts pending OS/Defender updates) — this check answers "is AV actually
running and current."

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `name` | string | Product display name. |
| `enabled` | bool | Real-time protection is on. |
| `up_to_date` | bool | Virus definitions are current. |
| `product_state` | int | Raw Security Center `productState` bitfield. |

`enabled`/`up_to_date` are decoded from `productState` using the well-known
heuristic (the `0x1000` bit = real-time protection on, the `0x10` bit = stale
definitions); the raw value is exposed as `product_state` for auditing. Default
threshold: **critical** if any product has `enabled = 0` or `up_to_date = 0`.
**Windows only.**
