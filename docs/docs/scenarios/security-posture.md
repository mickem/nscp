# Host Security Posture Monitoring

**Goal:** Monitor a host's security posture — certificate expiry and TLS hygiene,
interactive logon sessions, Secure Boot, local accounts and administrator group
membership, file ownership and permissions, and (on Windows) the firewall,
antivirus, disk encryption and network-profile state — using the
`CheckSecurity` module.

`check_certificate`, `check_users`, `check_secureboot`, `check_local_accounts`,
`check_group_members` and `check_file_security` work on **Windows and Linux**;
the remaining checks (`check_firewall`, `check_antivirus`, `check_bitlocker`,
`check_nla`, `check_activation`) are **Windows-only** and report a clear "not
supported" on other platforms rather than a false OK.

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

## Secure Boot, Local Accounts and Admin Groups (Windows and Linux)

These three read the Windows registry, SAM and local groups on Windows, and the
UEFI variables, `/etc/passwd` + `/etc/shadow` and `/etc/group` on Linux.

### UEFI Secure Boot

`check_secureboot` reports whether Secure Boot is enabled. Use `supported` to
tell "disabled" from "legacy BIOS" (on Linux: no `/sys/firmware/efi`):

```
check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
OK: secure boot is enabled
```

On Linux `setup_mode = 1` additionally catches firmware with no Platform Key
enrolled.

### Local accounts

`check_local_accounts` is critical by default when an enabled account needs no
password. On Linux that means an empty password field; the `*` / `!` system
accounts are fine. Add a rule for a second superuser:

```
check_local_accounts "crit=(enabled = 1 and password_required = 0) or is_extra_uid0 = 1"
```

On Linux the password state lives in `/etc/shadow`, which the unprivileged
`nsclient` user cannot read: the check is then UNKNOWN until the agent can read
it (Debian: add `nsclient` to the `shadow` group) or `allow-unreadable-shadow`
is passed, which keeps only the `/etc/passwd` keywords meaningful.

### Administrator group membership

`check_group_members` lists a local group (default `Administrators` on Windows,
`sudo` or `wheel` on Linux) and is critical on any member not on the
`expected=` allow-list:

```
check_group_members expected=Administrator "expected=Domain Admins"
check_group_members expected=root expected=deploy        ; Linux
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

### Network Location Awareness

`check_nla` confirms networks are classified as expected — e.g. a domain machine
should not treat a connected network as `public`:

```
check_nla "crit=connected = 1 and category = 'public'"
OK: all networks ok
```

### Windows activation

`check_activation` reports whether Windows is licensed and how much of a grace
or KMS renewal period is left. An unactivated machine starts nagging users and
eventually restricts functionality, and a KMS client whose renewal quietly stops
working takes months to become visible any other way:

```
check_activation
OK: Windows(R), Professional edition: licensed (genuine, grace 0d)
```

### File and service-binary permissions

`check_file_security` reports the owner and the writers of a path — a folder,
or the binary behind a service. Alerts by default on a world-writable path or an
owner outside the expected list:

```
check_file_security "path=C:\ProgramData\app-data"
CRITICAL: C:\ProgramData\app-data: world writable by Everyone
```

```
check_file_security service=MyAgent "expected-owner=BUILTIN\Administrators" "expected-owner=NT SERVICE\TrustedInstaller"
OK: C:\Program Files\MyAgent\agent.exe: owner BUILTIN\Administrators, no unexpected write access
```

On Linux the same check reads the owner, the group, the mode bits and any POSIX
ACL. `root` and the `root` group may always write; so may an owner you name
with `expected-owner`:

```
check_file_security path=/etc/shadow expected-owner=root expected-group=shadow
OK: /etc/shadow: owner root:shadow mode 0640, no unexpected write access
```

`max-depth` looks inside a directory as well, and `mode` pins the exact
permission bits where "nobody unexpected can write" is not strict enough:

```
check_file_security path=/etc/myapp max-depth=2 expected-owner=root "top-syntax=${status}: ${problem_count} of ${count} path(s) have a problem: ${problem_list}"
CRITICAL: 1 of 5 path(s) have a problem: /etc/myapp/conf.d/extra.conf: world writable by other
```

```
check_file_security path=/etc/myapp/app.conf "critical=mode != '0600'" "detail-syntax=${path}: mode ${mode}"
CRITICAL: /etc/myapp/app.conf: mode 0644
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
alias_activation  = check_activation
alias_datadir_acl = check_file_security path=C:/ProgramData/app-data "expected-owner=BUILTIN\\Administrators"
```

On a **Linux** host the cross-platform subset applies:

```ini
[/settings/check helpers/alias]
alias_cert       = check_certificate file=/etc/ssl/certs recursive=true "crit=expired = 1 or expires_in < 10"
alias_users      = check_users "crit=count > 10"
alias_secureboot = check_secureboot "warn=supported = 0" "crit=supported = 1 and enabled = 0"
alias_accounts   = check_local_accounts "crit=(enabled = 1 and password_required = 0) or is_extra_uid0 = 1"
alias_sudoers    = check_group_members expected=root expected=deploy
alias_shadow     = check_file_security path=/etc/shadow expected-owner=root expected-group=shadow
alias_sshd       = check_file_security service=ssh path=/etc/ssh/sshd_config expected-owner=root
```

Each alias is then callable by name over NRPE
(`check_nrpe -H <agent> -c alias_cert`).

`allow arguments = false` is what keeps a caller from re-pointing these checks
at something else. If this agent also serves the REST API, give those clients
the built-in `restricted` web role rather than `monitoring` — it refuses any
request carrying arguments, so the same rule applies on both transports and the
aliases above stay the only thing a monitoring server can ask for. See
[Securing NSClient++](../setup/securing.md).

---

## Next Steps

- [Network Checks](network-checks.md) — HTTPS endpoint cert expiry (`ssl_expiry_days`) and TLS verification
- [Windows Server Health](windows-server-health.md) — combine posture checks with a system-health baseline
- [Active Monitoring with NRPE](nrpe.md) — expose these checks to your monitoring server
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — write filter and threshold expressions
