#### About `check_certificate`

`check_certificate` inspects X.509 certificates **at rest** and alerts on
expiry and validity. It reads certificates from:

* **Files on disk** (`file=`/`path=`) — PEM (including multi-certificate
  bundles) and DER. A directory is scanned (add `recursive=true` to descend).
  This works on **all platforms** (parsing is OpenSSL-based).
* **The Windows certificate store** (`store=`, e.g. `My`, `Root`, `CA`) at a
  `location=` of `LocalMachine` (default) or `CurrentUser`. **Windows only** —
  on other platforms `store=` returns a clear "not supported" message.

This is distinct from `check_http`'s `ssl_expiry_days`, which checks the
certificate a live TLS endpoint *serves*. Use `check_http` for "is my website's
cert expiring"; use `check_certificate` for certificates in the store or on
disk that are not served over HTTP (code-signing, RDP, client-auth, service
certs, CA certs, or files a service loads at start-up).

Default thresholds: **warn** when `expires_in < 30` days, **critical** when
`expires_in < 10` days.

Filter/threshold keywords:

| Keyword                          | Type   | Meaning                                                                        |
|----------------------------------|--------|--------------------------------------------------------------------------------|
| `expires_in` / `expires_in_days` | int    | Whole days until expiry (negative if expired). Emitted as perfdata (unit `d`). |
| `expires_in_sec`                 | int    | Seconds until expiry.                                                          |
| `expired`                        | bool   | True if already past the not-after date.                                       |
| `not_yet_valid`                  | bool   | True if the not-before date is in the future.                                  |
| `not_after` / `not_before`       | date   | The validity window boundaries.                                                |
| `subject` / `issuer`             | string | Distinguished names.                                                           |
| `thumbprint`                     | string | SHA-1 fingerprint (lower-case hex).                                            |
| `serial`                         | string | Serial number (hex).                                                           |
| `source`                         | string | File path or store descriptor the cert came from.                              |
| `store`                          | string | `file` or the Windows store descriptor.                                        |

When neither `file=` nor `store=` is given, or nothing matches, the check
returns UNKNOWN rather than a silent OK.

> **Note:** chain/trust validation (`trusted`) is not yet implemented; this
> release focuses on expiry and validity.
