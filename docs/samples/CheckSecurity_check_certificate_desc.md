#### About `check_certificate`

`check_certificate` inspects X.509 certificates **at rest** and alerts on
expiry, validity and TLS hygiene (weak keys/signatures, trust). It reads
certificates from:

* **Files on disk** (`file=`/`path=`) — PEM (including multi-certificate
  bundles), DER, and PKCS#12 (`.pfx`/`.p12`, with `password=`). A directory is
  scanned (add `recursive=true` to descend). This works on **all platforms**
  (parsing is OpenSSL-based).
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
| `trusted`                        | bool   | True if the cert chains to a trusted CA (`ca=` bundle, else the system store). Time validity is **ignored** here — use `expired`/`not_yet_valid` for that. |
| `self_signed`                    | bool   | True if the certificate is self-signed.                                        |
| `signature_algorithm`            | string | e.g. `sha256WithRSAEncryption`.                                                |
| `weak_signature`                 | bool   | True if signed with MD5 or SHA-1.                                              |
| `key_type`                       | string | Public key type (`RSA`, `EC`, `DSA`, …).                                       |
| `key_size`                       | int    | Public key size in bits.                                                       |
| `weak_key`                       | bool   | True if RSA/DSA < 2048 bits, or EC < 256 bits.                                 |

Options: `ca=` supplies a CA bundle for the `trusted` check (defaults to the
system trust store); `password=` unlocks a PKCS#12 file. When neither `file=`
nor `store=` is given, or nothing matches, the check returns UNKNOWN rather than
a silent OK.

> **Trust caveat:** `trusted` requires the issuing chain to be resolvable —
> either present in the same file/batch (e.g. a full-chain PEM or a `.pfx` that
> bundles its CA) or reachable from the `ca=`/system store. A lone leaf whose
> intermediates are absent will read as `trusted=0`. Chain time validity is not
> checked here by design; combine with `expired`.
