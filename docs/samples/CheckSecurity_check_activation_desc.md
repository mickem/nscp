#### About `check_activation`

`check_activation` reports the Windows activation and licensing state: whether
the installed product is licensed, how much of a grace or KMS renewal period is
left and whether Windows considers itself genuine. An expiring grace period is
worth knowing about before it ends — Windows starts nagging users, then blocks
personalisation and eventually restricts functionality.

The data comes from `SoftwareLicensingProduct` (WMI, `root\CIMV2`), limited to
products that have a product key installed. The genuine state is taken from the
same class where it is exposed and otherwise from `SLIsGenuineLocal` in
`slc.dll`; both are local calls that do not contact Microsoft.

By default only Windows itself is reported. `all-products=true` adds every other
licensed product with an installed key (Office, for instance).

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `name` | string | Product name, e.g. `Windows(R), Professional edition`. |
| `description` | string | Product description, including the licensing channel. |
| `id` | string | Product SKU id (GUID). |
| `key` | string | Partial product key: the last five characters of the installed key. |
| `channel` | string | Product key channel: `Retail`, `Volume:MAK`, `Volume:GVLK`, `OEM`, … |
| `status` | string | Licensing status as a word: `licensed`, `unlicensed`, `initial_grace`, `additional_grace`, `non_genuine_grace`, `notification`, `extended_grace`. |
| `genuine_state` | string | `genuine`, `invalid_license`, `tampered`, `offline` or `unknown`. |
| `licensed` | bool | True when the product is fully licensed (activated). |
| `genuine` | bool | True only when `genuine_state` is `genuine`; an undetermined state is **not** genuine. |
| `is_windows` | bool | True for Windows itself, false for other licensed products. |
| `license_status` | int | Raw `LicenseStatus`: 0 unlicensed, 1 licensed, 2 initial grace, 3 additional grace, 4 non-genuine grace, 5 notification, 6 extended grace. |
| `license_status_reason` | int | Raw `LicenseStatusReason` code explaining the status. |
| `grace_days` | int | Remaining grace/renewal period in whole days (perfdata, unit `d`). |
| `grace_minutes` | int | The same period in minutes, as Windows reports it. |

Options:

| Option | Default | Meaning |
|---|---|---|
| `all-products` | `false` | Report every licensed product with an installed key, not only Windows. |
| `skip-genuine` | `false` | Do not evaluate the genuine state; `genuine_state` then reads `unknown`. |

Default thresholds: **critical** when `licensed = 0` — that covers an unlicensed
machine as well as one running on a grace period or already in the notification
("activate Windows") state — and **warning** when
`grace_days > 0 and grace_days < 30`. A permanently activated machine reports
`grace_days = 0`, so the warning only fires where a countdown is actually
running: the out-of-box grace period, or a KMS client that has not managed to
renew. Note that a KMS client normally shows a large `grace_days` value (up to
180) even though it is perfectly healthy.

Only one row is expected, so the default perfdata label is the fixed word
`license`; pass `perf-syntax=${name}` (or `${key}`) to tell several products
apart when using `all-products=true`. **Windows only.**
