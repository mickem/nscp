# Host Security Posture Monitoring

**Goal:** Monitor a host's security posture — certificate expiry and TLS hygiene,
interactive logon sessions, and (on Windows) the firewall, antivirus, disk
encryption, Secure Boot and network-profile state — using the `CheckSecurity`
module.

`check_certificate` and `check_users` work on **Windows and Linux**; the
remaining checks (`check_firewall`, `check_antivirus`, `check_bitlocker`,
`check_secureboot`, `check_nla`) are **Windows-only** and report a clear
"not supported" on other platforms rather than a false OK.

---

## Prerequisites

Enable the `CheckSecurity` module in `nsclient.ini` (enabled under the same name
on both Windows and Linux):

```ini
[/modules]
CheckSecurity = enabled
NRPEServer    = enabled   ; if using NRPE (active monitoring)
```

---

## Certificate Expiry and Hygiene (cross-platform)

`check_certificate` inspects certificates **at rest** — files on disk (PEM, DER,
PKCS#12) on any platform, and the Windows certificate store. It alerts on expiry
by default (warn < 30 days, critical < 10) and exposes TLS-hygiene keywords.

```
check_certificate file=/etc/ssl/certs/mysite.pem
OK: all 1 certificate(s) are ok
```

Catch weak crypto or an untrusted chain in the same check:

```
check_certificate file=/etc/ssl/certs/mysite.pem "crit=expired = 1 or weak_signature = 1 or weak_key = 1"
check_certificate file=/etc/nginx/fullchain.pem "crit=not trusted or expires_in < 14"
```

On Windows, scan a store instead of a file:

```
check_certificate store=My "crit=expires_in < 14"
```

This is complementary to `check_http`'s `ssl_expiry_days` (which checks the cert
a live endpoint *serves*); use `check_certificate` for certs on disk / in the
store that aren't served over HTTP. See [Network Checks](network-checks.md) for
the HTTPS side.

---

## Logged-on Users (cross-platform)

`check_users` reports interactive sessions (WTS on Windows, utmp on Linux).
`count` is a built-in summary variable.

```
check_users
OK: 2 user(s) logged on: mickem, root
```

Alert on any session on a locked-down server, or on too many, or on a specific
session type:

```
check_users "crit=count > 0"
check_users "warn=count > 5" "crit=count > 10"
check_users "filter=session_type = 'rdp'" "crit=count > 0"   ; Windows RDP
check_users "filter=session_type = 'remote'" "crit=count > 0" ; Linux ssh
```

---

## Windows Posture Checks

These read Windows-specific facilities and are **Windows-only**.

### Firewall profiles

`check_firewall` asserts the Domain / Private / Public profiles are enabled
(critical if any is off by default):

```
check_firewall
OK: all 3 firewall profile(s) enabled
```

### Antivirus / Defender

`check_antivirus` reads the Windows Security Center — critical by default if any
product has real-time protection off or stale definitions:

```
check_antivirus
OK: 1 antivirus product(s) healthy
```

### BitLocker / disk encryption

`check_bitlocker` reports per-volume protection (critical if any volume is
unprotected). Filter to the drives that matter:

```
check_bitlocker "filter=drive = 'C:'" "crit=protected = 0"
OK: all 1 volume(s) protected
```

### UEFI Secure Boot

`check_secureboot` reports whether Secure Boot is enabled. Use `supported` to
tell "disabled" from "legacy BIOS":

```
check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
OK: secure boot is enabled
```

### Network Location Awareness

`check_nla` confirms networks are classified as expected — e.g. a domain machine
should not treat a connected network as `public`:

```
check_nla "crit=connected = 1 and category = 'public'"
OK: all networks ok
```

---

## Putting It All Together

A `CheckSecurity` posture baseline for a **Windows** host, with the checks
pre-bound as NRPE-callable aliases. Aliases live in the lightweight
`[/settings/check helpers/alias]` section (see [External Scripts](external-scripts.md)),
so enable `CheckHelpers` too:

```ini
[/modules]
CheckSecurity = enabled
CheckHelpers  = enabled          ; provides the alias section below
NRPEServer    = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1         ; your monitoring server
allow arguments = false          ; define checks below instead of passing args

[/settings/check helpers/alias]
alias_cert        = check_certificate store=My "crit=expired = 1 or expires_in < 10 or weak_signature = 1"
alias_users       = check_users "warn=count > 5" "crit=count > 10"
alias_firewall    = check_firewall
alias_antivirus   = check_antivirus
alias_bitlocker   = check_bitlocker "filter=drive = 'C:'" "crit=protected = 0"
alias_secureboot  = check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
```

On a **Linux** host the cross-platform subset applies:

```ini
[/settings/check helpers/alias]
alias_cert  = check_certificate file=/etc/ssl/certs recursive=true "crit=expired = 1 or expires_in < 10"
alias_users = check_users "crit=count > 10"
```

Each alias is then callable by name over NRPE
(`check_nrpe -H <agent> -c alias_cert`).

---

## Next Steps

- [Network Checks](network-checks.md) — HTTPS endpoint cert expiry (`ssl_expiry_days`) and TLS verification
- [Windows Server Health](windows-server-health.md) — combine posture checks with a system-health baseline
- [Active Monitoring with NRPE](nrpe.md) — expose these checks to your monitoring server
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — write filter and threshold expressions
