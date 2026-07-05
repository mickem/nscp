# CheckSecurity

CheckSecurity checks host security posture: certificate expiry and (on Windows) the firewall profile state.

## Enable module

To enable this module and and allow using the commands you need to ass `CheckSecurity = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckSecurity = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckSecurity module.

**List of commands:**

A list of all available queries (check commands)
| Command                                 | Description                                                                                                  |
|-----------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [check_antivirus](#check_antivirus)     | Check registered antivirus products' enabled/up-to-date state (Windows Security Center). Windows only.       |
| [check_bitlocker](#check_bitlocker)     | Check BitLocker drive-encryption protection status per volume. Windows only.                                 |
| [check_certificate](#check_certificate) | Check X.509 certificate expiry/validity/hygiene from files (all platforms) or the Windows certificate store. |
| [check_firewall](#check_firewall)       | Check the Windows firewall profile (Domain/Private/Public) enabled state. Windows only.                      |
| [check_nla](#check_nla)                 | Check the Network Location Awareness profile (public/private/domain) per network. Windows only.              |
| [check_secureboot](#check_secureboot)   | Check whether UEFI Secure Boot is enabled. Windows only.                                                     |
| [check_users](#check_users)             | Check the count and detail of logged-on / RDP sessions (Windows and Linux).                                  |

### check_antivirus

Check registered antivirus products' enabled/up-to-date state (Windows Security Center). Windows only.

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


**Jump to section:**

* [Sample Commands](#check_antivirus_samples)
* [Command-line Arguments](#check_antivirus_options)
* [Filter keywords](#check_antivirus_filter_keys)


<a id="check_antivirus_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_antivirus_samples.md)_

#### Check that antivirus is enabled and up to date (Windows)

The default is critical if any registered product has real-time protection off
or stale definitions.

```
check_antivirus
L        cli OK: 1 antivirus product(s) healthy
```

```
check_antivirus
L        cli CRITICAL: Windows Defender (enabled=1 up_to_date=0)
```

#### Only require definitions to be current

```
check_antivirus "crit=up_to_date = 0"
L        cli OK: 1 antivirus product(s) healthy
```

#### Show each product's state

```
check_antivirus "top-syntax=${list}" "detail-syntax=${name}: enabled=${enabled} current=${up_to_date} state=${product_state}"
L        cli OK: Windows Defender: enabled=1 current=1 state=397568
```

#### On non-Windows platforms

```
check_antivirus
L        cli UNKNOWN: check_antivirus is not supported on this platform (Windows Security Center only)
```



<a id="check_antivirus_warn"></a>
<a id="check_antivirus_crit"></a>
<a id="check_antivirus_debug"></a>
<a id="check_antivirus_show-all"></a>
<a id="check_antivirus_escape-html"></a>
<a id="check_antivirus_help"></a>
<a id="check_antivirus_help-pb"></a>
<a id="check_antivirus_show-default"></a>
<a id="check_antivirus_help-short"></a>
<a id="check_antivirus_options"></a>
#### Command-line Arguments


| Option                                          | Default Value                                         | Description                                                                                                      |
|-------------------------------------------------|-------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_antivirus_filter)               |                                                       | Filter which marks interesting items.                                                                            |
| [warning](#check_antivirus_warning)             |                                                       | Filter which marks items which generates a warning state.                                                        |
| warn                                            |                                                       | Short alias for warning                                                                                          |
| [critical](#check_antivirus_critical)           | enabled = 0 or up_to_date = 0                         | Filter which marks items which generates a critical state.                                                       |
| crit                                            |                                                       | Short alias for critical.                                                                                        |
| [ok](#check_antivirus_ok)                       |                                                       | Filter which marks items which generates an ok state.                                                            |
| debug                                           | N/A                                                   | Show debugging information in the log                                                                            |
| show-all                                        | N/A                                                   | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_antivirus_empty-state)     | unknown                                               | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_antivirus_perf-config)     |                                                       | Performance data generation configuration                                                                        |
| escape-html                                     | N/A                                                   | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                            | N/A                                                   | Show help screen (this screen)                                                                                   |
| help-pb                                         | N/A                                                   | Show help screen as a protocol buffer payload                                                                    |
| show-default                                    | N/A                                                   | Show default values for a given command                                                                          |
| help-short                                      | N/A                                                   | Show help screen (short format).                                                                                 |
| [top-syntax](#check_antivirus_top-syntax)       | ${status}: ${list}                                    | Top level syntax.                                                                                                |
| [ok-syntax](#check_antivirus_ok-syntax)         | ${status}: ${count} antivirus product(s) healthy      | ok syntax.                                                                                                       |
| [empty-syntax](#check_antivirus_empty-syntax)   | No antivirus product registered                       | Empty syntax.                                                                                                    |
| [detail-syntax](#check_antivirus_detail-syntax) | ${name} (enabled=${enabled} up_to_date=${up_to_date}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_antivirus_perf-syntax)     | ${name}                                               | Performance alias syntax.                                                                                        |



<h5 id="check_antivirus_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_antivirus_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_antivirus_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `enabled = 0 or up_to_date = 0`

<h5 id="check_antivirus_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_antivirus_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_antivirus_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_antivirus_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_antivirus_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: ${count} antivirus product(s) healthy`

<h5 id="check_antivirus_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No antivirus product registered`

<h5 id="check_antivirus_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name} (enabled=${enabled} up_to_date=${up_to_date})`

<h5 id="check_antivirus_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_antivirus_filter_keys"></a>
#### Filter keywords


| Option        | Description                               |
|---------------|-------------------------------------------|
| enabled       | True if real-time protection is enabled   |
| name          | Antivirus product display name            |
| product_state | Raw Security Center productState bitfield |
| up_to_date    | True if virus definitions are current     |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



### check_bitlocker

Check BitLocker drive-encryption protection status per volume. Windows only.

#### About `check_bitlocker`

`check_bitlocker` reports the BitLocker protection state of each encryptable
volume (WMI `Win32_EncryptableVolume` in
`root\CIMV2\Security\MicrosoftVolumeEncryption`). Use it to confirm disk
encryption is actually on where policy requires it.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `drive` | string | Drive letter (may be empty for non-lettered volumes). |
| `protected` | bool | True when `protection_status == 1` (BitLocker on). |
| `protection_status` | int | Raw status: 0 off, 1 on, 2 unknown. |
| `conversion_status` | int | Raw conversion status (0 decrypted, 1 encrypted, …). |

Default threshold: **critical** if any volume has `protected = 0`. Filter to the
volumes you care about (e.g. `filter=drive = 'C:'`) to avoid alerting on
recovery/utility partitions. **Windows only.** Reading this class requires
elevation, so the agent service must run with sufficient privilege.


**Jump to section:**

* [Sample Commands](#check_bitlocker_samples)
* [Command-line Arguments](#check_bitlocker_options)
* [Filter keywords](#check_bitlocker_filter_keys)


<a id="check_bitlocker_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_bitlocker_samples.md)_

#### Check that all volumes are BitLocker-protected (Windows)

The default is critical if any encryptable volume is not protected.

```
check_bitlocker
L        cli OK: all 2 volume(s) protected
```

```
check_bitlocker
L        cli CRITICAL: D: protected=0
```

#### Only require the system drive to be protected

```
check_bitlocker "filter=drive = 'C:'" "crit=protected = 0"
L        cli OK: all 1 volume(s) protected
```

#### Show each volume's protection state

```
check_bitlocker "top-syntax=${list}" "detail-syntax=${drive} protected=${protected} status=${protection_status}"
L        cli OK: C: protected=1 status=1, D: protected=1 status=1
```

#### On non-Windows platforms

```
check_bitlocker
L        cli UNKNOWN: check_bitlocker is not supported on this platform (Windows BitLocker only)
```



<a id="check_bitlocker_warn"></a>
<a id="check_bitlocker_crit"></a>
<a id="check_bitlocker_debug"></a>
<a id="check_bitlocker_show-all"></a>
<a id="check_bitlocker_escape-html"></a>
<a id="check_bitlocker_help"></a>
<a id="check_bitlocker_help-pb"></a>
<a id="check_bitlocker_show-default"></a>
<a id="check_bitlocker_help-short"></a>
<a id="check_bitlocker_options"></a>
#### Command-line Arguments


| Option                                          | Default Value                               | Description                                                                                                      |
|-------------------------------------------------|---------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_bitlocker_filter)               |                                             | Filter which marks interesting items.                                                                            |
| [warning](#check_bitlocker_warning)             |                                             | Filter which marks items which generates a warning state.                                                        |
| warn                                            |                                             | Short alias for warning                                                                                          |
| [critical](#check_bitlocker_critical)           | protected = 0                               | Filter which marks items which generates a critical state.                                                       |
| crit                                            |                                             | Short alias for critical.                                                                                        |
| [ok](#check_bitlocker_ok)                       |                                             | Filter which marks items which generates an ok state.                                                            |
| debug                                           | N/A                                         | Show debugging information in the log                                                                            |
| show-all                                        | N/A                                         | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_bitlocker_empty-state)     | unknown                                     | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_bitlocker_perf-config)     |                                             | Performance data generation configuration                                                                        |
| escape-html                                     | N/A                                         | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                            | N/A                                         | Show help screen (this screen)                                                                                   |
| help-pb                                         | N/A                                         | Show help screen as a protocol buffer payload                                                                    |
| show-default                                    | N/A                                         | Show default values for a given command                                                                          |
| help-short                                      | N/A                                         | Show help screen (short format).                                                                                 |
| [top-syntax](#check_bitlocker_top-syntax)       | ${status}: ${list}                          | Top level syntax.                                                                                                |
| [ok-syntax](#check_bitlocker_ok-syntax)         | ${status}: all ${count} volume(s) protected | ok syntax.                                                                                                       |
| [empty-syntax](#check_bitlocker_empty-syntax)   | No encryptable volumes found                | Empty syntax.                                                                                                    |
| [detail-syntax](#check_bitlocker_detail-syntax) | ${drive} protected=${protected}             | Detail level syntax.                                                                                             |
| [perf-syntax](#check_bitlocker_perf-syntax)     | ${drive}                                    | Performance alias syntax.                                                                                        |



<h5 id="check_bitlocker_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_bitlocker_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_bitlocker_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `protected = 0`

<h5 id="check_bitlocker_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_bitlocker_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_bitlocker_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_bitlocker_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_bitlocker_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: all ${count} volume(s) protected`

<h5 id="check_bitlocker_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No encryptable volumes found`

<h5 id="check_bitlocker_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${drive} protected=${protected}`

<h5 id="check_bitlocker_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${drive}`


<a id="check_bitlocker_filter_keys"></a>
#### Filter keywords


| Option            | Description                                           |
|-------------------|-------------------------------------------------------|
| conversion_status | Raw conversion status (0 decrypted, 1 encrypted, ...) |
| drive             | Drive letter of the volume                            |
| protected         | True if BitLocker protection is on                    |
| protection_status | Raw protection status (0 off, 1 on, 2 unknown)        |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



### check_certificate

Check X.509 certificate expiry/validity/hygiene from files (all platforms) or the Windows certificate store.

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


**Jump to section:**

* [Sample Commands](#check_certificate_samples)
* [Command-line Arguments](#check_certificate_options)
* [Filter keywords](#check_certificate_filter_keys)


<a id="check_certificate_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_certificate_samples.md)_

#### Check a certificate file's expiry (default thresholds)

The default thresholds warn when a certificate expires within 30 days and go
critical within 10 days (matching common practice).

```
check_certificate file=/etc/ssl/certs/mysite.pem
L        cli OK: all 1 certificate(s) are ok
```

#### A certificate close to expiry trips the default critical

```
check_certificate file=/etc/ssl/certs/soon.pem
L        cli CRITICAL: /CN=soon.example.com expires in 5d (2026-07-09 19:16:12)
```

#### Custom thresholds and perfdata

`expires_in` is the number of whole days until expiry (negative once expired)
and is emitted as perfdata (unit `d`).

```
check_certificate file=/etc/ssl/certs/mysite.pem warning=expires_in<900
L        cli  Performance data: '/CN=valid.example.com'=825d;900;10
```

#### Scan a directory of certificates

```
check_certificate file=/etc/ssl/certs recursive=true "detail-syntax=${subject}: ${expires_in}d"
L        cli OK: all 137 certificate(s) are ok
```

#### Alert only on already-expired certificates

```
check_certificate file=/etc/pki/tls/certs critical=expired=1
L        cli OK: all 4 certificate(s) are ok
```

#### Flag weak keys or signatures (TLS hygiene)

```
check_certificate file=/etc/ssl/certs/mysite.pem "crit=weak_signature = 1 or weak_key = 1"
L        cli CRITICAL: /CN=legacy.example.com expires in 40d (2026-08-14 00:00:00)
```

Show the crypto detail for an audit:

```
check_certificate file=/etc/ssl/certs/mysite.pem "top-syntax=${list}" "detail-syntax=${subject}: ${signature_algorithm}, ${key_type}/${key_size}"
L        cli OK: /CN=mysite.example.com: sha256WithRSAEncryption, RSA/2048
```

#### Verify the certificate is trusted

`trusted` checks that the chain resolves to a trusted CA (time validity is
ignored — combine with `expired`). Point `ca=` at a bundle, or omit it to use the
system trust store:

```
check_certificate file=/etc/nginx/fullchain.pem "crit=not trusted or expired = 1"
check_certificate file=/etc/ssl/leaf.pem ca=/etc/ssl/corp-ca.pem "crit=not trusted"
```

#### Read a PKCS#12 (.pfx) file

```
check_certificate file=/opt/app/keystore.pfx password=changeit "crit=expires_in < 14"
L        cli OK: all 1 certificate(s) are ok
```

#### Windows certificate store (Windows only)

On Windows, `store=` enumerates a system certificate store; `location=` selects
`LocalMachine` (default) or `CurrentUser`.

```
check_certificate store=My location=LocalMachine
L        cli OK: all 6 certificate(s) are ok
```

```
check_certificate store=My "filter=subject like 'CN=*.example.com'" "crit=expires_in < 14"
L        cli WARNING: CN=web.example.com expires in 9d (2026-07-13 00:00:00)
```

On non-Windows platforms `store=` is rejected with a clear message:

```
check_certificate store=My
L        cli UNKNOWN: store= (certificate store) is only supported on Windows; use file= on this platform
```



<a id="check_certificate_warn"></a>
<a id="check_certificate_crit"></a>
<a id="check_certificate_debug"></a>
<a id="check_certificate_show-all"></a>
<a id="check_certificate_escape-html"></a>
<a id="check_certificate_help"></a>
<a id="check_certificate_help-pb"></a>
<a id="check_certificate_show-default"></a>
<a id="check_certificate_help-short"></a>
<a id="check_certificate_file"></a>
<a id="check_certificate_path"></a>
<a id="check_certificate_password"></a>
<a id="check_certificate_ca"></a>
<a id="check_certificate_store"></a>
<a id="check_certificate_options"></a>
#### Command-line Arguments


| Option                                            | Default Value                                      | Description                                                                                                      |
|---------------------------------------------------|----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_certificate_filter)               |                                                    | Filter which marks interesting items.                                                                            |
| [warning](#check_certificate_warning)             | expires_in < 30                                    | Filter which marks items which generates a warning state.                                                        |
| warn                                              |                                                    | Short alias for warning                                                                                          |
| [critical](#check_certificate_critical)           | expires_in < 10                                    | Filter which marks items which generates a critical state.                                                       |
| crit                                              |                                                    | Short alias for critical.                                                                                        |
| [ok](#check_certificate_ok)                       |                                                    | Filter which marks items which generates an ok state.                                                            |
| debug                                             | N/A                                                | Show debugging information in the log                                                                            |
| show-all                                          | N/A                                                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_certificate_empty-state)     | unknown                                            | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_certificate_perf-config)     |                                                    | Performance data generation configuration                                                                        |
| escape-html                                       | N/A                                                | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                              | N/A                                                | Show help screen (this screen)                                                                                   |
| help-pb                                           | N/A                                                | Show help screen as a protocol buffer payload                                                                    |
| show-default                                      | N/A                                                | Show default values for a given command                                                                          |
| help-short                                        | N/A                                                | Show help screen (short format).                                                                                 |
| [top-syntax](#check_certificate_top-syntax)       | ${status}: ${problem_list}                         | Top level syntax.                                                                                                |
| [ok-syntax](#check_certificate_ok-syntax)         | %(status): all %(count) certificate(s) are ok      | ok syntax.                                                                                                       |
| [empty-syntax](#check_certificate_empty-syntax)   | No certificates found                              | Empty syntax.                                                                                                    |
| [detail-syntax](#check_certificate_detail-syntax) | ${subject} expires in ${expires_in}d (${valid_to}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_certificate_perf-syntax)     | ${subject}                                         | Performance alias syntax.                                                                                        |
| file                                              |                                                    | A certificate file (PEM or DER) or a directory of them. Can be given multiple times.                             |
| path                                              |                                                    | Alias for file.                                                                                                  |
| [recursive](#check_certificate_recursive)         | 1)] (=0                                            | Recurse into directories given via file=/path=.                                                                  |
| password                                          |                                                    | Password for PKCS#12 (.pfx/.p12) files.                                                                          |
| ca                                                |                                                    | CA bundle to evaluate the 'trusted' keyword against (defaults to the system trust store).                        |
| store                                             |                                                    | Windows certificate store to enumerate (e.g. My, Root, CA). Windows only.                                        |
| [location](#check_certificate_location)           | LocalMachine                                       | Windows store location: LocalMachine or CurrentUser. Windows only.                                               |



<h5 id="check_certificate_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_certificate_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `expires_in < 30`

<h5 id="check_certificate_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `expires_in < 10`

<h5 id="check_certificate_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_certificate_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_certificate_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_certificate_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_certificate_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): all %(count) certificate(s) are ok`

<h5 id="check_certificate_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No certificates found`

<h5 id="check_certificate_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${subject} expires in ${expires_in}d (${valid_to})`

<h5 id="check_certificate_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${subject}`

<h5 id="check_certificate_recursive">recursive:</h5>

Recurse into directories given via file=/path=.

*Default Value:* `1)] (=0`

<h5 id="check_certificate_location">location:</h5>

Windows store location: LocalMachine or CurrentUser. Windows only.

*Default Value:* `LocalMachine`


<a id="check_certificate_filter_keys"></a>
#### Filter keywords


| Option              | Description                                                                                                                         |
|---------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| expired             | True if the certificate has already expired                                                                                         |
| expires_in          | Whole days until the certificate expires (negative if already expired)                                                              |
| expires_in_days     | Alias for expires_in                                                                                                                |
| expires_in_sec      | Seconds until the certificate expires (negative if already expired)                                                                 |
| issuer              | Certificate issuer distinguished name                                                                                               |
| key_size            | Public key size in bits                                                                                                             |
| key_type            | Public key type (RSA, EC, DSA, ...)                                                                                                 |
| not_after           | The not-after / expiry date                                                                                                         |
| not_before          | The not-before date                                                                                                                 |
| not_yet_valid       | True if the certificate is not yet valid (not-before is in the future)                                                              |
| self_signed         | True if the certificate is self-signed                                                                                              |
| serial              | Certificate serial number (hex)                                                                                                     |
| signature_algorithm | Signature algorithm (e.g. sha256WithRSAEncryption)                                                                                  |
| source              | Where the certificate was read from (file path or store)                                                                            |
| store               | The store/source type (file or a Windows store name)                                                                                |
| subject             | Certificate subject (e.g. /CN=host.example.com)                                                                                     |
| thumbprint          | SHA-1 fingerprint (lower-case hex)                                                                                                  |
| trusted             | True if the certificate chains to a trusted CA (system store or ca=; time validity is ignored — use expired/not_yet_valid for that) |
| valid_from          | Not-before date (UTC)                                                                                                               |
| valid_to            | Not-after / expiry date (UTC)                                                                                                       |
| weak_key            | True if the key is weak (RSA/DSA < 2048 bits, or EC < 256 bits)                                                                     |
| weak_signature      | True if signed with a weak algorithm (MD5 or SHA-1)                                                                                 |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



### check_firewall

=== "Windows"

    Check the Windows firewall profile (Domain/Private/Public) enabled state. Windows only.

    #### About `check_firewall`

    `check_firewall` reports the state of the **Windows firewall profiles**
    (Domain, Private, Public), the same data `Get-NetFirewallProfile` exposes. It
    reads them through the `INetFwPolicy2` COM interface, so it needs no WMI.

    Each profile is one row with these keywords:

    | Keyword    | Type   | Meaning                                                                |
    |------------|--------|------------------------------------------------------------------------|
    | `profile`  | string | Profile name: `Domain`, `Private` or `Public`.                         |
    | `enabled`  | bool   | True if the firewall is enabled for that profile. Emitted as perfdata. |
    | `active`   | bool   | True if the profile is currently applied to a connected network. Emitted as perfdata (`<profile> active`). |
    | `inbound`  | string | Default inbound action (`allow`/`block`).                              |
    | `outbound` | string | Default outbound action (`allow`/`block`).                             |

    Default threshold: **critical** if any profile has `enabled = 0`.

    `active` reflects which profile(s) Network Location Awareness currently
    applies (`INetFwPolicy2::CurrentProfileTypes`). More than one profile can be
    active when several networks are connected; with **no** connected network
    Windows reports the Public profile as active. Its main use is catching a
    machine silently dropping from `Domain`/`Private` to `Public` after a
    router/adapter change — firewall rules scoped to the domain or private
    profile stop applying and services start getting blocked. This is opt-in via
    a `warn`/`crit` expression (see the samples) since being on the public
    profile is perfectly normal for e.g. laptops.

    This check is **Windows only**. It models Windows' fixed three-profile firewall,
    which does not map onto Linux firewalls (firewalld zones, ufw, nftables/iptables
    default policies); on non-Windows platforms it returns UNKNOWN with a clear
    message rather than pretending to check something equivalent.


    **Jump to section:**

    * [Sample Commands](#check_firewall_samples)
    * [Command-line Arguments](#check_firewall_options)
    * [Filter keywords](#check_firewall_filter_keys)


    <a id="check_firewall_samples"></a>
    #### Sample Commands

    _To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_firewall_samples.md)_

    #### Check that all Windows firewall profiles are enabled (Windows only)

    By default the check is critical if any of the three profiles (Domain, Private,
    Public) has its firewall disabled.

    ```
    check_firewall
    L        cli OK: all 3 firewall profile(s) enabled
    ```

    ```
    check_firewall
    L        cli CRITICAL: Public=0
    ```

    #### Only require a specific profile to be enabled

    ```
    check_firewall "filter=profile = 'Domain'" crit=enabled=0
    L        cli OK: all 1 firewall profile(s) enabled
    ```

    #### Warn when the machine is on the Public profile

    Network Location Awareness can silently re-categorise a network to *public*
    after a router or connection change; rules scoped to the domain/private
    profiles then stop applying and services get blocked. Warn on that (opt-in —
    on e.g. laptops the public profile is normal):

    ```
    check_firewall "warn=active = 1 and profile = 'Public'" "detail-syntax=${profile} profile is active"
    L        cli WARNING: Public profile is active
    ```

    The `active` flags are also available for display and perfdata:

    ```
    check_firewall "detail-syntax=${profile}: enabled=${enabled} active=${active}" top-syntax=${list}
    L        cli OK: Domain: enabled=1 active=0, Private: enabled=1 active=0, Public: enabled=1 active=1
    ```

    #### Show the default inbound/outbound actions

    ```
    check_firewall "detail-syntax=${profile}: enabled=${enabled} in=${inbound} out=${outbound}" top-syntax=${list}
    L        cli OK: Domain: enabled=1 in=block out=allow, Private: enabled=1 in=block out=allow, Public: enabled=1 in=block out=allow
    ```

    #### On non-Windows platforms

    `check_firewall` models the Windows three-profile firewall and is not
    implemented on Linux (whose firewalld/ufw/nftables model differs):

    ```
    check_firewall
    L        cli UNKNOWN: check_firewall is not supported on this platform (Windows-only; the Linux firewall model differs)
    ```



    <a id="check_firewall_warn"></a>
    <a id="check_firewall_crit"></a>
    <a id="check_firewall_debug"></a>
    <a id="check_firewall_show-all"></a>
    <a id="check_firewall_escape-html"></a>
    <a id="check_firewall_help"></a>
    <a id="check_firewall_help-pb"></a>
    <a id="check_firewall_show-default"></a>
    <a id="check_firewall_help-short"></a>
    <a id="check_firewall_options"></a>
    #### Command-line Arguments


    | Option                                         | Default Value                                       | Description                                                                                                      |
    |------------------------------------------------|-----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_firewall_filter)               |                                                     | Filter which marks interesting items.                                                                            |
    | [warning](#check_firewall_warning)             |                                                     | Filter which marks items which generates a warning state.                                                        |
    | warn                                           |                                                     | Short alias for warning                                                                                          |
    | [critical](#check_firewall_critical)           | enabled = 0                                         | Filter which marks items which generates a critical state.                                                       |
    | crit                                           |                                                     | Short alias for critical.                                                                                        |
    | [ok](#check_firewall_ok)                       |                                                     | Filter which marks items which generates an ok state.                                                            |
    | debug                                          | N/A                                                 | Show debugging information in the log                                                                            |
    | show-all                                       | N/A                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_firewall_empty-state)     | unknown                                             | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_firewall_perf-config)     |                                                     | Performance data generation configuration                                                                        |
    | escape-html                                    | N/A                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                           | N/A                                                 | Show help screen (this screen)                                                                                   |
    | help-pb                                        | N/A                                                 | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                   | N/A                                                 | Show default values for a given command                                                                          |
    | help-short                                     | N/A                                                 | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_firewall_top-syntax)       | ${status}: ${problem_list}                          | Top level syntax.                                                                                                |
    | [ok-syntax](#check_firewall_ok-syntax)         | %(status): all %(count) firewall profile(s) enabled | ok syntax.                                                                                                       |
    | [empty-syntax](#check_firewall_empty-syntax)   | No firewall profiles found                          | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_firewall_detail-syntax) | ${profile}=${enabled}                               | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_firewall_perf-syntax)     | ${profile}                                          | Performance alias syntax.                                                                                        |



    <h5 id="check_firewall_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_firewall_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_firewall_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `enabled = 0`

    <h5 id="check_firewall_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_firewall_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_firewall_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_firewall_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${problem_list}`

    <h5 id="check_firewall_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): all %(count) firewall profile(s) enabled`

    <h5 id="check_firewall_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No firewall profiles found`

    <h5 id="check_firewall_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${profile}=${enabled}`

    <h5 id="check_firewall_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${profile}`


    <a id="check_firewall_filter_keys"></a>
    #### Filter keywords


    | Option   | Description                                                                                                                                    |
    |----------|------------------------------------------------------------------------------------------------------------------------------------------------|
    | active   | True if the profile is currently applied to a connected network (e.g. NLA re-categorising a network to public makes the Public profile active) |
    | enabled  | True if the profile's firewall is enabled                                                                                                      |
    | inbound  | Default inbound action (allow/block)                                                                                                           |
    | outbound | Default outbound action (allow/block)                                                                                                          |
    | profile  | Firewall profile name (Domain, Private or Public)                                                                                              |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

=== "Linux"

    Check the Windows firewall profile (Domain/Private/Public) enabled state. Windows only.

    #### About `check_firewall`

    `check_firewall` reports the state of the **Windows firewall profiles**
    (Domain, Private, Public), the same data `Get-NetFirewallProfile` exposes. It
    reads them through the `INetFwPolicy2` COM interface, so it needs no WMI.

    Each profile is one row with these keywords:

    | Keyword    | Type   | Meaning                                                                |
    |------------|--------|------------------------------------------------------------------------|
    | `profile`  | string | Profile name: `Domain`, `Private` or `Public`.                         |
    | `enabled`  | bool   | True if the firewall is enabled for that profile. Emitted as perfdata. |
    | `active`   | bool   | True if the profile is currently applied to a connected network. Emitted as perfdata (`<profile> active`). |
    | `inbound`  | string | Default inbound action (`allow`/`block`).                              |
    | `outbound` | string | Default outbound action (`allow`/`block`).                             |

    Default threshold: **critical** if any profile has `enabled = 0`.

    `active` reflects which profile(s) Network Location Awareness currently
    applies (`INetFwPolicy2::CurrentProfileTypes`). More than one profile can be
    active when several networks are connected; with **no** connected network
    Windows reports the Public profile as active. Its main use is catching a
    machine silently dropping from `Domain`/`Private` to `Public` after a
    router/adapter change — firewall rules scoped to the domain or private
    profile stop applying and services start getting blocked. This is opt-in via
    a `warn`/`crit` expression (see the samples) since being on the public
    profile is perfectly normal for e.g. laptops.

    This check is **Windows only**. It models Windows' fixed three-profile firewall,
    which does not map onto Linux firewalls (firewalld zones, ufw, nftables/iptables
    default policies); on non-Windows platforms it returns UNKNOWN with a clear
    message rather than pretending to check something equivalent.


    **Jump to section:**

    * [Sample Commands](#check_firewall_samples)
    * [Command-line Arguments](#check_firewall_options)
    * [Filter keywords](#check_firewall_filter_keys)


    <a id="check_firewall_samples"></a>
    #### Sample Commands

    _To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_firewall_samples.md)_

    #### Check that all Windows firewall profiles are enabled (Windows only)

    By default the check is critical if any of the three profiles (Domain, Private,
    Public) has its firewall disabled.

    ```
    check_firewall
    L        cli OK: all 3 firewall profile(s) enabled
    ```

    ```
    check_firewall
    L        cli CRITICAL: Public=0
    ```

    #### Only require a specific profile to be enabled

    ```
    check_firewall "filter=profile = 'Domain'" crit=enabled=0
    L        cli OK: all 1 firewall profile(s) enabled
    ```

    #### Warn when the machine is on the Public profile

    Network Location Awareness can silently re-categorise a network to *public*
    after a router or connection change; rules scoped to the domain/private
    profiles then stop applying and services get blocked. Warn on that (opt-in —
    on e.g. laptops the public profile is normal):

    ```
    check_firewall "warn=active = 1 and profile = 'Public'" "detail-syntax=${profile} profile is active"
    L        cli WARNING: Public profile is active
    ```

    The `active` flags are also available for display and perfdata:

    ```
    check_firewall "detail-syntax=${profile}: enabled=${enabled} active=${active}" top-syntax=${list}
    L        cli OK: Domain: enabled=1 active=0, Private: enabled=1 active=0, Public: enabled=1 active=1
    ```

    #### Show the default inbound/outbound actions

    ```
    check_firewall "detail-syntax=${profile}: enabled=${enabled} in=${inbound} out=${outbound}" top-syntax=${list}
    L        cli OK: Domain: enabled=1 in=block out=allow, Private: enabled=1 in=block out=allow, Public: enabled=1 in=block out=allow
    ```

    #### On non-Windows platforms

    `check_firewall` models the Windows three-profile firewall and is not
    implemented on Linux (whose firewalld/ufw/nftables model differs):

    ```
    check_firewall
    L        cli UNKNOWN: check_firewall is not supported on this platform (Windows-only; the Linux firewall model differs)
    ```



    <a id="check_firewall_warn"></a>
    <a id="check_firewall_crit"></a>
    <a id="check_firewall_debug"></a>
    <a id="check_firewall_show-all"></a>
    <a id="check_firewall_escape-html"></a>
    <a id="check_firewall_help"></a>
    <a id="check_firewall_help-pb"></a>
    <a id="check_firewall_show-default"></a>
    <a id="check_firewall_help-short"></a>
    <a id="check_firewall_options"></a>
    #### Command-line Arguments


    | Option                                         | Default Value                                       | Description                                                                                                      |
    |------------------------------------------------|-----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_firewall_filter)               |                                                     | Filter which marks interesting items.                                                                            |
    | [warning](#check_firewall_warning)             |                                                     | Filter which marks items which generates a warning state.                                                        |
    | warn                                           |                                                     | Short alias for warning                                                                                          |
    | [critical](#check_firewall_critical)           | enabled = 0                                         | Filter which marks items which generates a critical state.                                                       |
    | crit                                           |                                                     | Short alias for critical.                                                                                        |
    | [ok](#check_firewall_ok)                       |                                                     | Filter which marks items which generates an ok state.                                                            |
    | debug                                          | N/A                                                 | Show debugging information in the log                                                                            |
    | show-all                                       | N/A                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_firewall_empty-state)     | unknown                                             | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_firewall_perf-config)     |                                                     | Performance data generation configuration                                                                        |
    | escape-html                                    | N/A                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                           | N/A                                                 | Show help screen (this screen)                                                                                   |
    | help-pb                                        | N/A                                                 | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                   | N/A                                                 | Show default values for a given command                                                                          |
    | help-short                                     | N/A                                                 | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_firewall_top-syntax)       | ${status}: ${problem_list}                          | Top level syntax.                                                                                                |
    | [ok-syntax](#check_firewall_ok-syntax)         | %(status): all %(count) firewall profile(s) enabled | ok syntax.                                                                                                       |
    | [empty-syntax](#check_firewall_empty-syntax)   | No firewall profiles found                          | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_firewall_detail-syntax) | ${profile}=${enabled}                               | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_firewall_perf-syntax)     | ${profile}                                          | Performance alias syntax.                                                                                        |



    <h5 id="check_firewall_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_firewall_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_firewall_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `enabled = 0`

    <h5 id="check_firewall_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_firewall_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_firewall_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_firewall_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${problem_list}`

    <h5 id="check_firewall_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).

    *Default Value:* `%(status): all %(count) firewall profile(s) enabled`

    <h5 id="check_firewall_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No firewall profiles found`

    <h5 id="check_firewall_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${profile}=${enabled}`

    <h5 id="check_firewall_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${profile}`


    <a id="check_firewall_filter_keys"></a>
    #### Filter keywords


    | Option   | Description                                       |
    |----------|---------------------------------------------------|
    | enabled  | True if the profile's firewall is enabled         |
    | inbound  | Default inbound action (allow/block)              |
    | outbound | Default outbound action (allow/block)             |
    | profile  | Firewall profile name (Domain, Private or Public) |

    **Common options for all checks:**

    | Option        | Description                                                                    |
    |---------------|--------------------------------------------------------------------------------|
    | count         | Number of items matching the filter.                                           |
    | crit_count    | Number of items matched the critical criteria.                                 |
    | crit_list     | A list of all items which matched the critical criteria.                       |
    | detail_list   | A special list with critical, then warning and finally ok.                     |
    | list          | A list of all items which matched the filter.                                  |
    | ok_count      | Number of items matched the ok criteria.                                       |
    | ok_list       | A list of all items which matched the ok criteria.                             |
    | problem_count | Number of items matched either warning or critical criteria.                   |
    | problem_list  | A list of all items which matched either the critical or the warning criteria. |
    | status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
    | total         | Total number of items.                                                         |
    | warn_count    | Number of items matched the warning criteria.                                  |
    | warn_list     | A list of all items which matched the warning criteria.                        |

### check_nla

Check the Network Location Awareness profile (public/private/domain) per network. Windows only.

#### About `check_nla`

`check_nla` reports the **Network Location Awareness** profile of each network
the machine knows about (the same classification Windows uses to pick a firewall
profile), via the COM `INetworkListManager` interface. It is a security-posture
check: confirm a domain-joined machine is on the `domain` category and not
accidentally treating a network as `private`/`public`.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `network` | string | Network name. |
| `category` | string | `public`, `private` or `domain`. |
| `connected` | bool | True if the network is currently connected. |

There is no default threshold — assert the expected posture, e.g.
`crit=connected = 1 and category != 'domain'`. **Windows only**; on other
platforms it returns UNKNOWN with a clear message (Linux has no equivalent
network-profile concept).


**Jump to section:**

* [Sample Commands](#check_nla_samples)
* [Command-line Arguments](#check_nla_options)
* [Filter keywords](#check_nla_filter_keys)


<a id="check_nla_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_nla_samples.md)_

#### Assert the active network is the domain profile (Windows)

`check_nla` reports each network's Location Awareness category. There is no
default threshold — assert the posture you expect:

```
check_nla "crit=connected = 1 and category != 'domain'" "detail-syntax=${network}=${category}"
L        cli OK: all networks ok
```

#### Alert if any connected network is classified Public

```
check_nla "crit=connected = 1 and category = 'public'"
L        cli CRITICAL: Wi-Fi=public
```

#### List every known network and its category

```
check_nla "top-syntax=${list}" "detail-syntax=${network}: ${category} (connected=${connected})"
L        cli OK: Corp.example.com: domain (connected=1), Café-WiFi: public (connected=0)
```

#### On non-Windows platforms

```
check_nla
L        cli UNKNOWN: check_nla is not supported on this platform (Windows Network Location Awareness only)
```



<a id="check_nla_warn"></a>
<a id="check_nla_crit"></a>
<a id="check_nla_debug"></a>
<a id="check_nla_show-all"></a>
<a id="check_nla_escape-html"></a>
<a id="check_nla_help"></a>
<a id="check_nla_help-pb"></a>
<a id="check_nla_show-default"></a>
<a id="check_nla_help-short"></a>
<a id="check_nla_options"></a>
#### Command-line Arguments


| Option                                    | Default Value              | Description                                                                                                      |
|-------------------------------------------|----------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_nla_filter)               |                            | Filter which marks interesting items.                                                                            |
| [warning](#check_nla_warning)             |                            | Filter which marks items which generates a warning state.                                                        |
| warn                                      |                            | Short alias for warning                                                                                          |
| [critical](#check_nla_critical)           |                            | Filter which marks items which generates a critical state.                                                       |
| crit                                      |                            | Short alias for critical.                                                                                        |
| [ok](#check_nla_ok)                       |                            | Filter which marks items which generates an ok state.                                                            |
| debug                                     | N/A                        | Show debugging information in the log                                                                            |
| show-all                                  | N/A                        | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_nla_empty-state)     | ok                         | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_nla_perf-config)     |                            | Performance data generation configuration                                                                        |
| escape-html                               | N/A                        | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                      | N/A                        | Show help screen (this screen)                                                                                   |
| help-pb                                   | N/A                        | Show help screen as a protocol buffer payload                                                                    |
| show-default                              | N/A                        | Show default values for a given command                                                                          |
| help-short                                | N/A                        | Show help screen (short format).                                                                                 |
| [top-syntax](#check_nla_top-syntax)       | ${status}: ${list}         | Top level syntax.                                                                                                |
| [ok-syntax](#check_nla_ok-syntax)         | ${status}: all networks ok | ok syntax.                                                                                                       |
| [empty-syntax](#check_nla_empty-syntax)   | No networks found          | Empty syntax.                                                                                                    |
| [detail-syntax](#check_nla_detail-syntax) | ${network}=${category}     | Detail level syntax.                                                                                             |
| [perf-syntax](#check_nla_perf-syntax)     | ${network}                 | Performance alias syntax.                                                                                        |



<h5 id="check_nla_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_nla_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_nla_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_nla_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_nla_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_nla_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_nla_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_nla_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: all networks ok`

<h5 id="check_nla_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No networks found`

<h5 id="check_nla_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${network}=${category}`

<h5 id="check_nla_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${network}`


<a id="check_nla_filter_keys"></a>
#### Filter keywords


| Option    | Description                                 |
|-----------|---------------------------------------------|
| category  | Network category: public, private or domain |
| connected | True if the network is currently connected  |
| network   | Network name                                |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



### check_secureboot

Check whether UEFI Secure Boot is enabled. Windows only.

#### About `check_secureboot`

`check_secureboot` reports whether **UEFI Secure Boot** is enabled, read from the
registry value
`HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State\UEFISecureBootEnabled`
(the same source `Confirm-SecureBootUEFI` uses). It returns a single result.

Keywords:

| Keyword | Type | Meaning |
|---|---|---|
| `enabled` | bool | True if UEFI Secure Boot is enabled. |
| `supported` | bool | True if the platform exposes a Secure Boot state (UEFI); false on legacy BIOS boots. |

Default threshold: **critical** if `enabled = 0`. On legacy-BIOS machines the
value is absent, so both `enabled` and `supported` are 0 — use `supported` to
tell "off" apart from "not applicable" on mixed fleets. **Windows only.**


**Jump to section:**

* [Sample Commands](#check_secureboot_samples)
* [Command-line Arguments](#check_secureboot_options)
* [Filter keywords](#check_secureboot_filter_keys)


<a id="check_secureboot_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_secureboot_samples.md)_

#### Check that UEFI Secure Boot is enabled (Windows)

The default is critical if Secure Boot is not enabled.

```
check_secureboot
L        cli OK: secure boot is enabled
```

```
check_secureboot
L        cli CRITICAL: secure boot enabled=0 supported=1
```

#### Distinguish "disabled" from "not supported" (legacy BIOS)

`supported` is 0 when the platform does not expose a Secure Boot state (legacy
BIOS boot). Treat that as WARNING rather than CRITICAL if you monitor mixed
fleets:

```
check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
L        cli WARNING: secure boot enabled=0 supported=0
```

#### On non-Windows platforms

```
check_secureboot
L        cli UNKNOWN: check_secureboot is not supported on this platform (Windows/UEFI only)
```



<a id="check_secureboot_warn"></a>
<a id="check_secureboot_crit"></a>
<a id="check_secureboot_debug"></a>
<a id="check_secureboot_show-all"></a>
<a id="check_secureboot_escape-html"></a>
<a id="check_secureboot_help"></a>
<a id="check_secureboot_help-pb"></a>
<a id="check_secureboot_show-default"></a>
<a id="check_secureboot_help-short"></a>
<a id="check_secureboot_options"></a>
#### Command-line Arguments


| Option                                           | Default Value                                         | Description                                                                                                      |
|--------------------------------------------------|-------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_secureboot_filter)               |                                                       | Filter which marks interesting items.                                                                            |
| [warning](#check_secureboot_warning)             |                                                       | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                                       | Short alias for warning                                                                                          |
| [critical](#check_secureboot_critical)           | enabled = 0                                           | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                                       | Short alias for critical.                                                                                        |
| [ok](#check_secureboot_ok)                       |                                                       | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                                   | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                                   | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_secureboot_empty-state)     | unknown                                               | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_secureboot_perf-config)     |                                                       | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                                   | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                                   | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                                   | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                                   | Show default values for a given command                                                                          |
| help-short                                       | N/A                                                   | Show help screen (short format).                                                                                 |
| [top-syntax](#check_secureboot_top-syntax)       | ${status}: ${list}                                    | Top level syntax.                                                                                                |
| [ok-syntax](#check_secureboot_ok-syntax)         | ${status}: secure boot is enabled                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_secureboot_empty-syntax)   | No Secure Boot state                                  | Empty syntax.                                                                                                    |
| [detail-syntax](#check_secureboot_detail-syntax) | secure boot enabled=${enabled} supported=${supported} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_secureboot_perf-syntax)     | secureboot                                            | Performance alias syntax.                                                                                        |



<h5 id="check_secureboot_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_secureboot_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_secureboot_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `enabled = 0`

<h5 id="check_secureboot_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_secureboot_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_secureboot_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_secureboot_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_secureboot_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: secure boot is enabled`

<h5 id="check_secureboot_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No Secure Boot state`

<h5 id="check_secureboot_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `secure boot enabled=${enabled} supported=${supported}`

<h5 id="check_secureboot_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `secureboot`


<a id="check_secureboot_filter_keys"></a>
#### Filter keywords


| Option    | Description                                             |
|-----------|---------------------------------------------------------|
| enabled   | True if UEFI Secure Boot is enabled                     |
| supported | True if the platform reports a Secure Boot state (UEFI) |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



### check_users

Check the count and detail of logged-on / RDP sessions (Windows and Linux).

#### About `check_users`

`check_users` reports the interactive logon sessions on the host — count and
per-session detail. It works on **both platforms**:

* **Windows** — via the Remote Desktop Services (WTS) API, so it distinguishes
  console from RDP and active from disconnected sessions (no WMI needed).
* **Linux** — via the utmp database (the same source as `who`); network logins
  (ssh) carry the remote host in `client`.

Filter/threshold keywords (plus the built-in `count` summary variable):

| Keyword | Type | Meaning |
|---|---|---|
| `count` | int | Number of matching sessions (built-in summary variable). |
| `user` | string | Account name. |
| `session_state` | string | `active`, `disconnected`, `connected`, … (Windows). Linux logins are always `active`. |
| `session_type` | string | `console`, `rdp`, `remote`, `ica`, … |
| `client` | string | Client name (Windows) or remote host (Linux); empty for local console. |

There is **no default threshold** — this is a count/inventory check, so supply
your own, e.g. `crit=count > 10` or `crit=session_state = 'disconnected'`.
Sessions with no user (services, the RDP listener) are not counted.


**Jump to section:**

* [Sample Commands](#check_users_samples)
* [Command-line Arguments](#check_users_options)
* [Filter keywords](#check_users_filter_keys)


<a id="check_users_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSecurity_check_users_samples.md)_

#### Count the logged-on users

```
check_users
L        cli OK: 2 user(s) logged on: mickem, root
```

#### Alert when too many sessions are open

`count` is a built-in summary variable.

```
check_users "warn=count > 5" "crit=count > 10"
L        cli OK: 2 user(s) logged on: mickem, root
```

#### Alert on any interactive session (e.g. a locked-down server)

```
check_users "crit=count > 0"
L        cli CRITICAL: 2 user(s) logged on: mickem, root
```

#### Only count RDP / remote sessions

```
check_users "filter=session_type = 'rdp'" "crit=count > 0"
```

On Linux, network logins (ssh) have `session_type = 'remote'`:

```
check_users "filter=session_type = 'remote'" "detail-syntax=${user}@${client}" "top-syntax=${list}"
L        cli OK: mickem@10.0.0.5
```

#### Alert on a disconnected-but-open RDP session (Windows)

```
check_users "crit=session_state = 'disconnected'" "detail-syntax=${user} (${session_state})"
```



<a id="check_users_warn"></a>
<a id="check_users_crit"></a>
<a id="check_users_debug"></a>
<a id="check_users_show-all"></a>
<a id="check_users_escape-html"></a>
<a id="check_users_help"></a>
<a id="check_users_help-pb"></a>
<a id="check_users_show-default"></a>
<a id="check_users_help-short"></a>
<a id="check_users_options"></a>
#### Command-line Arguments


| Option                                      | Default Value                                  | Description                                                                                                      |
|---------------------------------------------|------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_users_filter)               |                                                | Filter which marks interesting items.                                                                            |
| [warning](#check_users_warning)             |                                                | Filter which marks items which generates a warning state.                                                        |
| warn                                        |                                                | Short alias for warning                                                                                          |
| [critical](#check_users_critical)           |                                                | Filter which marks items which generates a critical state.                                                       |
| crit                                        |                                                | Short alias for critical.                                                                                        |
| [ok](#check_users_ok)                       |                                                | Filter which marks items which generates an ok state.                                                            |
| debug                                       | N/A                                            | Show debugging information in the log                                                                            |
| show-all                                    | N/A                                            | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_users_empty-state)     | ok                                             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_users_perf-config)     |                                                | Performance data generation configuration                                                                        |
| escape-html                                 | N/A                                            | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                        | N/A                                            | Show help screen (this screen)                                                                                   |
| help-pb                                     | N/A                                            | Show help screen as a protocol buffer payload                                                                    |
| show-default                                | N/A                                            | Show default values for a given command                                                                          |
| help-short                                  | N/A                                            | Show help screen (short format).                                                                                 |
| [top-syntax](#check_users_top-syntax)       | ${status}: ${count} user(s) logged on: ${list} | Top level syntax.                                                                                                |
| [ok-syntax](#check_users_ok-syntax)         | ${status}: ${count} user(s) logged on          | ok syntax.                                                                                                       |
| [empty-syntax](#check_users_empty-syntax)   | No users logged on                             | Empty syntax.                                                                                                    |
| [detail-syntax](#check_users_detail-syntax) | ${user}                                        | Detail level syntax.                                                                                             |
| [perf-syntax](#check_users_perf-syntax)     | ${user}                                        | Performance alias syntax.                                                                                        |



<h5 id="check_users_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_users_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_users_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_users_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_users_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_users_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_users_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${count} user(s) logged on: ${list}`

<h5 id="check_users_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: ${count} user(s) logged on`

<h5 id="check_users_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No users logged on`

<h5 id="check_users_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${user}`

<h5 id="check_users_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${user}`


<a id="check_users_filter_keys"></a>
#### Filter keywords


| Option        | Description                               |
|---------------|-------------------------------------------|
| client        | Client name or remote host (may be empty) |
| session_state | Session state (active, disconnected, ...) |
| session_type  | Session type (console, rdp, remote, ...)  |
| user          | The account name of the logged-on user    |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |



