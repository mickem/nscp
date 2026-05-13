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

> TODO: Add example of configuring TLS

## Http(s)/TLS

> TODO: Add example of configuring TLS
> TODO: Add example of configuring permissions

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
nscp settings --update
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

1. There is an entry `check_this = <path> <args>` under `[/settings/external scripts]` (or `/wrapped scripts`, or
   `/alias`), **and**
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

Direct script entries under `[/settings/external scripts]` (without a wrapping) are unaffected by clearing the wrappings
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
  below). If you want WEB up for monitoring but not for administration, set `disable admin user = true` under
  `[/settings/WEB/server]` — the admin account is then never created or activated, and an attacker who recovers the
  password hash cannot use it to log in. See *Disabling the admin user* below.
- **`allow arguments = true` + a script that doesn't validate its input** is the classic injection vector. Leave
  `allow arguments = false` unless there's a concrete reason to enable it, and never combine with
  `allow nasty characters = true` unless you fully trust both the calling monitoring server and every script the agent
  might invoke.

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
| Service account                                 | `LocalSystem` | dedicated low-privilege account with only the access your checks require |
