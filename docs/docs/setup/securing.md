# Securing NSClient++

## Protocols

NSClient++ supports a multitude of protocols thus securing the server will depend on the protocol you are using.

## NRPE

For details on setting up and using NRPE please see the [Active Monitoring with NRPE](../scenarios/nrpe.md).

In general when using NRPE do not use NRPE version 2 with the ADH key and do not rely on allowed hosts as the security
mechanism.
Instead, certificates and proper two-way TLS are preferred.

To set up NRPE with two-way TLS you need to:

1. Create a CA (Certificate Authority) or use an existing one.
2. Create a server certificate for the NSClient++ server signed by the CA.
3. Create a client certificate for the monitoring server signed by the CA.
4. Configure NSClient++ to use the server certificate and trust the CA.
5. Configure the monitoring server to use the client certificate and trust the CA.

Step 1-3 will depend on your environment and is covered in the [Active Monitoring with NRPE](../scenarios/nrpe.md).
Step 4 can easily be setup with from the command line like so:

```commandline
$ nscp nrpe install ^
	--allowed-hosts 127.0.0.1 ^
	--insecure=false --verify=peer-cert ^
	--certificate nsclient.pem ^
	--certificate-key nsclient.key ^
	--ca ca.pem
```

A quick breakdown of the options:

* `--allowed-hosts`: List of allowed hosts to connect from.
* `--insecure`: If true, do not verify the server certificate.
* `--verify`: What to verify, can be `none`, `peer-cert`.
* `--certificate`: Path to the server certificate.
* `--certificate-key`: Path to the server certificate key.
* `--ca`: Path to the CA certificate.

## NRDP

NRDP is an HTTP-based **passive** submission protocol (typically Nagios XI). The agent is the *client* — it does not
accept incoming NRDP traffic. Securing NRDP therefore means:

1. Make sure the agent talks to the NRDP server over HTTPS.
2. Keep the shared token out of cleartext config.
3. Restrict which checks may be relayed through NRDP using the [Permission policy](#permission-policy) so a
   compromised caller cannot use the relay to flood the upstream monitor.

There is no `nscp nrdp install` command — NRDP is configured by editing the target block. A minimal hardened target:

```ini
[/modules]
NRDPClient = enabled

[/settings/NRDP/client/targets/default]
; Always use https - the token is sent on the wire.
address  = https://nagios.example.com/nrdp/
ssl      = true
; Token / password are the same thing in NRDP - either key works.
; Move this into credential manager (see Passwords above) so it is not in
; cleartext beside the config.
token    = $CRED$
```

After editing, apply with:

```commandline
$ nscp service --restart
```

If you want to verify a target before relying on it, submit a synthetic passive result with:

```commandline
$ nscp nrdp --module submit --target default --host myhost --command check_ok --message "test"
```

## Http/Https/WebServer

The WEB module exposes the REST API and the web UI over HTTP or HTTPS on port `8443` by default. In any production
deployment you want HTTPS, a dedicated non-admin user for the monitoring server, and (where possible) the built-in
`admin` account disabled.

### Install with HTTPS from the command line

The fastest way to get a hardened install is the WEB module's own install command:

```commandline
$ nscp web install ^
    --https ^
    --allowed-hosts 10.0.0.0/24 ^
    --certificate nsclient.pem ^
    --certificate-key nsclient.key ^
    --port 8443
```

A quick breakdown of the options:

* `--https`: Enable HTTPS (without this it serves cleartext on port 8080).
* `--allowed-hosts`: CIDR or comma-separated list of source IPs allowed to connect.
* `--certificate` / `--certificate-key`: TLS server cert and private key.
* `--port`: Listening port (default `8443`).
* `--password`: Admin password. If omitted, a random one is generated and printed.
* `--disable-admin`: Lock out the built-in admin user — no admin row is created, the REST script-upload endpoint
  becomes unreachable. Recommended for monitoring-only deployments. Mutually exclusive with `--password`; create a
  dedicated user separately (see below).

For a monitoring-only deployment (no remote reconfiguration through the WEB UI), combine `--disable-admin` with a
dedicated user:

```commandline
$ nscp web install ^
    --https ^
    --allowed-hosts 10.0.0.0/24 ^
    --certificate nsclient.pem ^
    --certificate-key nsclient.key ^
    --disable-admin
$ nscp web add-user monitoring --role monitoring --password "<strong password>"
```

### Adding a dedicated user

`nscp web add-user` creates or updates a per-user row under `[/settings/WEB/server/users/<name>]`:

```commandline
$ nscp web add-user monitoring --role monitoring --password "<strong password>"
$ nscp web add-user dashboard  --role client
```

Options:

* `--user`: The username (positional `<name>` also works).
* `--password`: Plaintext password. Hashed before being written to the config. If omitted, a random one is generated
  and printed once — copy it then.
* `--role`: Built-in roles shipped by the module:
    * `monitoring` — `queries.execute, login.get, metrics.get`. Recommended for monitoring servers and Prometheus
      scrapes.
    * `client` — adds query listing; needed for the legacy `check_nscp_api` integration.
    * `full` — admin (settings, modules, scripts). Avoid for monitoring callers.
* `--grant`: Additional grants beyond the role (see `add-role` to define new roles).

Move the per-user passwords into the credential manager rather than leaving them in the INI; see the Passwords section
below.

### Locking down which commands the web user can run

The role controls *which REST endpoints* a user can hit. To control *which check commands* the user is allowed to
invoke through `/queries/{command}/commands/execute`, layer the [Permission policy](#permission-policy) on top:

```
[/settings/permissions/policies]
WEBServer:admin      = *
WEBServer:monitoring = CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckDisk.check_drivesize
```

For deeper coverage of the WEB module's attack surface — including the `--disable-admin` trade-off and the
`scripts_controller` endpoint — see the [WEB module](#web-module) section further down.

## check_nt (legacy NSClient protocol)

The `NSClientServer` module implements the original NSClient / check_nt protocol (TCP `12489` by default), spoken by the
Nagios `check_nt` plugin. **It is a dead protocol and should be avoided.** It predates modern transport security and
cannot be made genuinely secure. If you have any choice at all, monitor over [NRPE with two-way TLS](#nrpe) or the
[REST API](#httphttpswebserver) instead, and simply do not load `NSClientServer`.

> **Treat check_nt as unauthenticated cleartext.** Anyone who can observe a single request on the wire learns the
> password and can replay it forever. Do not expose it on any network you do not fully trust, and never reuse the
> check_nt password anywhere else.

Why it is not secure:

- **The password is sent in cleartext on every request.** check_nt prepends the configured password as the first field
  of every query. The server has no working TLS path in practice (the `ssl` toggle exists, but virtually no check_nt
  client ever implemented it), so the password travels unencrypted. The module logs a warning to this effect on startup
  whenever a password is configured.
- **There is no replay protection.** Unlike NSCA there is no timestamp, nonce or sequence number — a captured request
  can be replayed verbatim, indefinitely. Capturing the password once is equivalent to knowing it permanently.
- **A single shared secret is the only authentication.** There is no per-client identity, no certificate and no mutual
  auth. `allowed hosts` is source-IP filtering, not authentication, and is spoofable on an untrusted network.

What it does **not** expose, to be fair:

- **It cannot run arbitrary commands.** The protocol's ten request codes map to a *fixed* set of read-only internal
  checks (client version, CPU load, uptime, disk usage, memory, service/process state, performance counters, file age).
  The caller cannot choose the command name, cannot inject extra arguments, and there is no path to a shell or to
  `CheckExternalScripts`. The realistic risk is therefore **credential capture and read access to system metrics**, not
  remote code execution. The server-side authentication itself is sound (constant-time password compare, an empty
  password is refused, a single generic error string with no username/oracle) — it is the *transport* that is broken,
  and that cannot be fixed within the protocol.

If you must keep it running for a legacy monitoring system:

- Always configure a password (an empty password is refused outright) and **firewall TCP `12489` to the monitoring host
  only** — `allowed hosts` is defence-in-depth, not a control.
- Use a password dedicated to check_nt and nowhere else, since it is effectively public on any shared network segment.
- **Restrict which commands are answered with the `allow` setting** (see below) so that the effectively-public password
  can only read what you actually monitor.
- Plan the migration to NRPE or REST; do not build new monitoring on check_nt.

### Restricting which commands are answered (`allow`)

Because the password should be assumed compromised, the useful question is *how much can a holder of it read?* The
`allow` setting under `[/settings/NSClient/server]` caps that. It is a comma-separated list where each entry is a group,
the keyword `any`/`all`, or an individual command name:

| Group       | Commands                                       | Disclosure                                        |
|-------------|------------------------------------------------|---------------------------------------------------|
| `metrics`   | `cpuload`, `uptime`, `useddiskspace`, `memuse` | aggregate system metrics — harmless               |
| `info`      | `clientversion`                                | agent version (minor recon)                       |
| `service`   | `servicestate`                                 | service inventory (`ShowAll` lists all services)  |
| `process`   | `procstate`                                    | process inventory (`ShowAll` lists all processes) |
| `counters`  | `counter`, `instances`                         | **arbitrary** performance-counter read            |
| `files`     | `fileage`                                      | **arbitrary** file existence / last-modified time |
| `any`/`all` | everything                                     | full check_nt compatibility (**default**)         |

The default is `any`, which answers all ten commands (unchanged legacy behaviour). To expose only the harmless system
metrics — denying the arbitrary-read commands (`counter`, `fileage`, `instances`) and the service/process enumeration:

```ini
[/settings/NSClient/server]
; Only answer harmless system-metric queries plus the agent version.
allow = metrics, info
```

Mix groups and individual commands freely, e.g. `allow = metrics, info, servicestate` to also answer service-state
checks but nothing else. A request for a command outside the list is rejected (the server returns
`ERROR: Command not allowed.`); an empty/typo'd list logs a warning and rejects everything, so use `allow = any` to get
the default back.

To remove the attack surface entirely, just don't load the module:

```ini
[/modules]
; NSClientServer = enabled   ; leave disabled / removed - prefer NRPE or REST
```

## User account

By default, NSClient++ is running as `local system` which is a easy as it is always available.
A more secure approach is to use a dedicated user account.
This user account should be a local user account with only relevant admin right given the things you check.

> TODO: Add example of creating a user account and setting up NSClient++ to use it.
> TODO: Add example of permissions needed for various checks.

## Passwords

NSClient++ has among other secrets an admin password which out-of-the box is stored in a config file.
This is insecure and not recommended.
There are two simple way to solve this:

1. Store the config file in the profile of the user.
2. Store secrets in credential manager.

### Storing config in user profile

Default the service is using the `local system` account (if you are using a different account please update the paths in
below example).
To move the config file to `C:\Windows\System32\Config\systemprofile\NSClient++`.

First create the folder:

```commandline
$ PsExec -i -s cmd /c mkdir "C:\Windows\System32\Config\systemprofile\NSClient++"
```

Then move the settings file:

```commandline
$ PsExec -i -s "c:\program files\nsclient++\nscp" settings --migrate-to "ini:C:\Windows\System32\Config\systemprofile\NSClient++\nsclient.ini"
```

What this will do is update `boot.ini` to point to the new location and move all settings from the old config file to
the new one.

And finally delete the old config file:

```commandline
$ del "C:\Program Files\NSClient++\nsclient.ini"
```

Restart NSClient++ to make sure the new config file is used.

### Using credential manager

Using credential manager is easy you simply set:

```ini
[/settings]

; use credential manager - Store sensitive keys in use credential manager instead of ini file
use credential manager = true
```

And then save the config.

```commandline
nscp service --restart
```

Now here is the first catch, credential manager is per account so likely you are not running NSClient++ as the same user
as the one you are logged in as.
Thus you can either need to first switch to that user or run the command as that user.
The default account use is local system and to switch to local system you can
use [PsExec](https://docs.microsoft.com/en-us/sysinternals/downloads/psexec) like this:

```commandline
PsExec -i -s "c:\program files\nsclient++\nscp" settings --update
```

If you no open the ini file it will look like this:

```ini
[/settings/default]

; Password - Password used to authenticate against server
password = $CRED$; Se credential manager: NSClient++-/settings/default.password
```

Restart NSClient++ and you are good to go.

To restore passwords you can do the reverse:

```commandline
PsExec -i -s "c:\program files\nsclient++\nscp" settings --path /settings --key "use credential manager" --set false
PsExec -i -s "c:\program files\nsclient++\nscp" settings --update
```

## Permission policy

NSClient++ has a core-level permission layer that decides whether a given caller may run a given command. It is
**disabled by default** for backwards compatibility, but in any deployment that exposes more than one transport — or
that wants to limit what individual web users can do — enabling it is **strongly recommended** and is the single
biggest hardening step you can take after TLS.

The rest of this page focuses on transport-level controls (who can connect, with what credential). The permission
policy is the *next* layer: once the caller is authenticated, **what commands are they allowed to run?** Without it,
any caller that can reach a transport can invoke every command that module exposes.

When enabled, the policy is a **strict allow-list** — calls that don't match any rule are denied. Rules name a subject
(the calling module, optionally with a principal such as the authenticated web user) and a list of objects (the
`module.command` patterns they may invoke). Identity is stamped server-side by the trusted core; modules cannot forge
it. `CheckHelpers` forwards the original caller through wrap-and-dispatch commands so policies apply through proxy
chains, not just at the outermost hop.

A minimal hardened policy:

```ini
[/settings/permissions]
enabled = true
log denials = true

[/settings/permissions/policies]
; NRPE may run a small fixed set of checks.
NRPEServer = CheckHelpers.*, CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckDisk.check_drivesize

; Scheduler runs whatever the operator configures locally.
Scheduler = *

; Web admin can do anything; web monitoring user can only run a fixed set.
WEBServer : admin   = *
WEBServer : monitor = CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckDisk.check_drivesize
```

### Recommended rollout

1. Enable in **observe mode** first: set `enabled = true`, add a single permissive rule `* = *`, and set
   `log allows = true`. NSClient++ now logs every call without blocking anything — use this to inventory what
   actually runs.
2. Replace `* = *` with rules that mirror the observed traffic.
3. Turn `log allows` back off (it is noisy) and keep `log denials = true`.
4. Test through proxy commands such as `check_multi` and `check_always_ok` to confirm policies still apply to the
   wrapped command (they do — `CheckHelpers` forwards the original caller).

See [Permissions](../concepts/permissions.md) for the full reference: identity model, which modules stamp what,
pattern syntax, the worked `CheckHelpers` example, and the detailed step-by-step setup guide.

## Remote code execution: understanding the attack surface

NSClient++ is, by design, a remote-administration agent. Several modules can ultimately cause arbitrary code to run on
the host. Whether that is acceptable depends on **who can reach the agent**, **how they authenticate**, and **what is in
the configuration**. The risk lives at the intersection of those three — not in any single module.

### CheckExternalScripts

This is the module most often flagged in security reviews because the name is self-describing. The short answer:

> **Enabling `CheckExternalScripts` does not let a remote caller run arbitrary commands.** It only exposes scripts that
> an administrator has *already declared* in the configuration. A monitoring client cannot say "run `evil.ps1`" — they
> can only say "run the alias `check_this`", and only if that alias points at something the local administrator wired
> up.

In detail, when the monitoring server asks the agent to run `check_this`, the agent will only run it if:

1. There is an entry `check_this = <path> <args>` under `[/settings/external scripts/scripts]` (or `/wrapped scripts`,
   or `/alias`), **and**
2. The configured script lives under the configured `script root` (default `${scripts}`, i.e. the install's `scripts\`
   folder), **and**
3. If the request includes arguments, `allow arguments = true` is also set (default `false`), **and**
4. If those arguments contain shell metacharacters, `allow nasty characters = true` is also set (default `false`).

The default posture is that arguments and shell metacharacters are **both rejected**. Even with
`allow arguments = true`, the agent only substitutes arguments into the *already-configured* command line — the command
itself is not user-controllable.

```ini
[/settings/external scripts]
allow arguments = false             ; default; clients cannot pass extra args
allow nasty characters = false      ; default; reject | & < > ` ' " \ [ ] { }
script root = ${scripts}            ; scripts must live here

[/settings/external scripts/scripts]
check_this = scripts\my_script.ps1  ; declared by the admin; required for it to be callable
```

#### Disabling specific wrappings

Disabling wrapping will not really impact anything as they do not do anything unless a script with the given wrapping is
added. That said, they can be set to `""`  if you want to remove them.

Whether you want to do this depends on what you actually run. If your monitoring playbooks only use PowerShell, removing
the `bat` and `vbs` wrappings might theoretically shrinks the attack surface at zero cost. If you don't use any shipped
wrapping at all, you can clear `ps1` too — `[/wrapped scripts]` entries with no matching wrapping will simply fail to
execute.

Direct script entries under `[/settings/external scripts/scripts/scripts]` are unaffected by clearing the wrappings
map — those use the literal command line you declared.

#### Aliases — also available in CheckHelpers

The alias mechanism that lets you build composite commands like

```ini
[/settings/external scripts/alias]
my_check_cpu = check_cpu warn=load>87% crit=load>92%
```

These commands are internal and cannot execute scripts (unless they are an alias for another script definition).
So using alias by themselves does not impact security.
Historically aliases were avalible via `CheckExternalScripts` since 0.12.4 `CheckHelpers` also provide the same
functionalty under its own independent section `[/settings/check helpers/alias]`:

```ini
[/settings/check helpers/alias]
my_check_cpu = check_cpu warn=load>87% crit=load>92%
```

The two sections are **independent** and you can use either one where `CheckHelpers` can be used without enabling
`ExternalScripts`.
The reason this matters for security: `CheckHelpers` only runs internal commands. It has no script path, no wrappings,
no `allow arguments` flag, no shell-execution code. Its attack surface is meaningfully smaller than
`CheckExternalScripts`. For environments where `CheckExternalScripts` is not apoproved, this is the recommended home
for aliases.

Aliases call internal commands (`check_cpu` etc.) with fixed arguments, which is how they avoid needing
`allow arguments = true` — the arguments are baked into the alias definition, not supplied by the client. `$ARG1$` /
`%ARG1%` substitution into the alias's *pre-declared* argument list is supported, but the alias's command name and
template are not caller-controllable.

Practical guidance:

- **Want aliases only?** Enable `CheckHelpers`, disable `CheckExternalScripts`, define aliases under
  `[/settings/check helpers/alias]`. Copy across any existing aliases from `[/settings/external scripts/alias]` by hand.
- **Want aliases plus external scripts?** Enable both. Each module reads its own section; pick one as the home for new
  aliases and stick with it to avoid duplicate definitions.

#### When is `CheckExternalScripts` actually risky?

The module becomes a real risk in these scenarios — none of which are about the module itself:

- **Anyone with write access to `nsclient.ini` can declare new commands.** Treat the INI as a privileged file; lock its
  NTFS ACLs to the service user and administrators only. See "Storing config in user profile" above.
- **Anyone with write access to the `scripts\` folder can change what runs** even for pre-declared commands. Lock that
  folder the same way.
- **The admin password can give equivalent power** via the WEB module's script-upload endpoint (see the WEB section
  below). If you want WEB ui for monitoring but not for administration, set `disable admin user = true` under
  `[/settings/WEB/server]` — the admin account is then never created or activated, and an attacker who recovers the
  password hash cannot use it to log in. See *Disabling the admin user* below.
- **`allow arguments = true` + a script that doesn't validate its input** is the classic injection vector. Leave
  `allow arguments = false` unless there's a concrete reason to enable it, and never combine with
  `allow nasty characters = true` unless you fully trust both the calling monitoring server and every script the agent
  might invoke.

Enabling the [permission policy](#permission-policy) is an effective additional layer here: even if a caller can reach
the transport, the policy decides whether `CheckExternalScripts.*` is callable from that subject at all, and you can
limit specific subjects to a small fixed set of script aliases.

### WEB module

The WEB module serves two distinct purposes, and the security guidance depends on which one you actually need:

- **As a monitoring endpoint** — exposing queries, metrics, and the OpenMetrics scrape target over HTTPS. No
  host-changing operations required.
- **As an administration UI** — letting an authenticated operator reconfigure the agent, including uploading new scripts
  that get executed by `CheckExternalScripts` / `PythonScript` (the `scripts_controller` endpoint at
  `PUT /api/v1/scripts/{runtime}/{name}`).

In monitoring-only deployments — by far the more common use — you can get the first set of capabilities **without** the
second.

#### Recommended for monitoring-only deployments

The fastest way to set this up is the WEB module's own install command, with the `--disable-admin` flag:

```commandline
$ nscp web install ^
    --https ^
    --allowed-hosts 10.0.0.0/24 ^
    --certificate nsclient.pem ^
    --certificate-key nsclient.key ^
    --disable-admin
$ nscp web add-user monitoring ^
    --role monitoring ^
    --password "$(openssl rand -base64 32)"
```

With `--disable-admin`, the install command does three things differently:

1. Sets `disable admin user = true` under `[/settings/WEB/server]`.
2. Skips writing the per-user `admin` row.
3. Creates a `monitoring` user with role `monitoring` and the supplied (or generated) password.

The equivalent INI written by hand:

```ini
[/settings/WEB/server]
disable admin user = true

[/settings/WEB/server/users/monitoring]
role = monitoring
password = <hash>
```

The `monitoring` role is registered by the WEB module at startup and grants only
`public,queries.execute,login.get,metrics.get` — enough for a monitoring server to log in, run queries and scrape
metrics, and nothing else. No `settings.*`, no `modules.*`, no `scripts.*`. If you need more (e.g. the legacy
`check_nscp_api` integration that lists queries), prefer the `client` role over `full`.

With `disable admin user = true`, the agent never creates or activates the `admin` account. The script-upload path is
still wired up in the code, but no account can authenticate to it — so even an attacker who recovers the
`/settings/default/password` cannot turn it into remote code execution. See *Disabling the admin user* below for what
the flag does and doesn't do.

Apply the usual transport / network hygiene on top:

- Use a real TLS certificate (do not let the agent silently fall back to HTTP on port 8080).
- Firewall the WEB port (`8443`) to your monitoring network only.
- Move the `/settings/default/password` into the credential manager so it's not sitting in cleartext alongside the
  config (see the Passwords section above).

#### When you actually need administration over WEB

If you genuinely want to administer the agent through the web UI — push config changes, upload scripts, restart
modules — keep the admin user enabled and treat its password the way you would a local-administrator credential on the
box:

- Rotate it on a defined schedule.
- Store the hash in the credential manager, not the INI.
- Restrict source IPs to a small set of administration jump hosts.
- Pair with reverse-proxy authentication (mTLS, SSO header, etc.) for defence in depth — the agent's own auth becomes
  the second factor.

**The honest framing is**: anyone with that password can run arbitrary code as `LocalSystem`. Manage it accordingly, or
remove the capability with the flag above.

#### When you don't need WEB at all

NRPE / NSCA / Graphite / check_mk monitoring flows do not require the WEB module. If your monitoring server doesn't poll
the REST API and you don't use the web UI, **don't load `WEBServer`**. The smallest attack surface is one that doesn't
exist.

### NRPE without two-way TLS

NRPE is the most common attack surface because it is the most commonly exposed. The default configuration is *
*TLS-encrypted but client-unauthenticated** — the agent verifies its own identity to the caller (via DH or the agent
certificate) but **does not verify the caller's identity**. Combined with `allow arguments = true`, this means:

> **Anyone who can reach TCP/5666 can invoke any command the agent is configured to expose**, with whatever arguments
> they choose (subject to `allow nasty characters`).

`allowed hosts` is **network filtering, not authentication**. It checks the source IP, which is trivially spoofable on
untrusted networks and trivially bypassable from any host inside the allowed range.

The robust answer is two-way TLS, where the agent verifies the client's certificate against a CA you control. See
the [NRPE](#nrpe) section earlier on this page for the command-line setup.

If two-way TLS is not yet in place, the compensating controls are:

- Keep `allow arguments = false` so callers can only invoke commands as declared, with the arguments declared.
- Use aliases (above) for any check that needs varying thresholds, instead of accepting arguments from the network.
- Restrict the network path to the agent (host firewall, dedicated VLAN, jump host).
- Treat `allowed hosts` as a defence-in-depth aid, not a security control.
- Enable the [permission policy](#permission-policy) and restrict `NRPEServer` to the exact list of commands the
  monitoring server actually invokes — this caps the blast radius even if a caller bypasses transport controls.

### Quick checklist

| Concern                                         | Default       | Recommended posture                                                      |
|-------------------------------------------------|---------------|--------------------------------------------------------------------------|
| `CheckExternalScripts` `allow arguments`        | `false`       | leave `false` unless you have a specific need                            |
| `CheckExternalScripts` `allow nasty characters` | `false`       | leave `false`                                                            |
| Unused wrappings (`bat`, `vbs`, etc.)           | shipped       | clear (`bat =`) if you don't use them                                    |
| `nsclient.ini` ACLs                             | install dir   | lock to service user + admins; or move under systemprofile               |
| `scripts\` folder ACLs                          | install dir   | lock to service user + admins                                            |
| Admin password storage                          | plain in INI  | move to credential manager                                               |
| NRPE `verify mode`                              | `none`        | `peer-cert` with a CA you control                                        |
| NRPE `allowed hosts`                            | broad         | tighten, but do not rely on it as the only control                       |
| `WEBServer` module                              | optional      | leave disabled if not actively used; tight ACLs and TLS if used          |
| `WEBServer` `disable admin user`                | `false`       | `true` if the WEB module is used only for monitoring, never for admin    |
| check_nt (`NSClientServer`) protocol            | optional      | avoid; leave disabled. If required, firewall to the monitor, treat the password as public, and set `allow = metrics, info` |
| Service account                                 | `LocalSystem` | dedicated low-privilege account with only the access your checks require |
| Permission policy (`/settings/permissions`)     | disabled      | enable in observe mode, lock down to per-subject allow-list              |
