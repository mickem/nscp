# Permissions

NSClient++ has a policy layer in the core that decides whether a given caller may run a given command. It is **disabled
by default** — every existing install continues to work without changes until an operator opts in.

When enabled, the policy is a **strict allow-list**: a call is permitted only if some rule grants it. Calls that don't
match any rule are denied.

This document covers what the system does, which modules participate in it, how `CheckHelpers` forwards identity, and a
step-by-step recipe for turning it on safely.

---

## The big idea

Every command call into the core carries two pieces of identity:

| Field       | What it means                                                                                                                    |
|-------------|----------------------------------------------------------------------------------------------------------------------------------|
| `module`    | The **calling module** — `WEBServer`, `NRPEServer`, `Scheduler`, etc. Resolved server-side from the trusted plugin registry.     |
| `principal` | An optional **sub-identity** within the module: the authenticated web user, an NRPE client tag, an NSCA sender, the OS CLI user. |

Together these form the **subject** of a permission decision, rendered as
`module[:principal]`:

```
WEBServer
WEBServer:operator
NRPEServer:icinga
Scheduler
CLI:admin
*
```

The **object** is the command being called, rendered as
`module.command`:

```
CheckSystem.check_cpu
CheckHelpers.check_ok
*
```

A **rule** says "this subject may run these objects." Rules combine additively — if any rule grants the call, it is
allowed.

```ini
[/settings/permissions/policies]
NRPEServer = CheckHelpers.*, CheckSystem.check_cpu, CheckSystem.check_drivesize
WEBServer : admin  = *
WEBServer : viewer = CheckSystem.check_cpu, CheckSystem.check_drivesize
Scheduler = CheckHelpers.*, CheckSystem.*
```

---

## Identity model: who stamps what

When you enable the policy, the core needs to know who is calling. The calling module's identity is stamped onto the
request automatically by the `nscapi::core_helper` library that every in-tree module uses. The *principal* is optional —
modules that have one supply it; modules that don't leave it empty.

| Module                               | `module` (always)      | `principal` (when known)                                            |
|--------------------------------------|------------------------|---------------------------------------------------------------------|
| **WEBServer**                        | `WEBServer`            | The authenticated web user from the session cookie (`uid`)          |
| **NRPEServer**                       | `NRPEServer`           | None today — wire up when "client tag" or remote-IP is set          |
| **NSClientServer** (legacy check_nt) | `NSClientServer`       | None today                                                          |
| **NSCAServer**                       | `NSCAServer`           | None today — sender id from the NSCA header is a natural future fit |
| **Scheduler**                        | `Scheduler`            | None today — schedule alias is a natural future fit                 |
| **PythonScript**                     | `PythonScript`         | None — scripts are trusted code, not delegated principals           |
| **LUAScript**                        | `LUAScript`            | None — same reason as Python                                        |
| **CheckHelpers**                     | (varies — see below)   | (varies — see below)                                                |
| **CheckExternalScripts**             | `CheckExternalScripts` | None today                                                          |

The module name is **not** something the calling DLL can fake: the library always stamps the real plugin id, and the
core resolves it to the module name using its own trusted registry.

### What about modules that don't issue commands?

Modules that only emit metrics or receive submissions (`CheckMKServer`, `GraphiteClient`, `IcingaClient`,
`NSCAClient`, …) do not appear as subjects at all. They never call into other modules — they only consume results.

---

## Special case: `CheckHelpers`

`CheckHelpers` is a **proxy module**: it wraps another command and returns the wrapped result.
`check_always_ok check_critical message=fire` is a single inbound call that internally runs `check_critical` and
rewrites the return code.

If `CheckHelpers` stamped its own identity on the wrapped call, the policy decision on `check_critical` would lose the
original caller — every call would look like `CheckHelpers` no matter who triggered it.
That defeats the purpose of having per-user policies.

So `CheckHelpers` reads the **inbound** caller identity from the request header and forwards it on the wrapped command.
The policy on the wrapped command sees the **original** caller, not the proxy.

### Worked example

`operator` is a WEBServer user. They call:

```
check_always_ok check_critical message=fire
```

Two policy decisions happen:

1. **First hop** — the inbound call to `CheckHelpers.check_always_ok`. Subject `WEBServer:operator`, object
   `CheckHelpers.check_always_ok`. A rule like `WEBServer:operator = CheckHelpers.*` grants this.

2. **Wrapped hop** — `CheckHelpers` internally dispatches `check_critical`. Without identity forwarding, this would be
   `CheckHelpers → CheckHelpers.check_critical`. **With forwarding** (the current behaviour), it is
   `WEBServer:operator → CheckHelpers.check_critical`. The same rule still grants it.

What this means in practice: write rules for *who is calling NSClient++* (WEBServer:operator, NRPEServer, Scheduler),
not for *which proxy module handles the call*. The operator's allow-list applies through the proxy chain.

### Which `CheckHelpers` commands forward identity

Every command that wraps another check forwards identity:

- `check_always_ok`, `check_always_warning`, `check_always_critical`
- `check_multi`
- `check_negate`
- `check_timeout`
- `check_and_forward`
- `filter_perf`, `render_perf`, `xform_perf`
- aliases (anything defined under `[/settings/check helpers/alias]`)

The trivial leaf commands (`check_ok`, `check_warning`, `check_critical`, `check_version`) don't dispatch and so don't
forward anything.

### When forwarding falls back

If `CheckHelpers` is called by a path that doesn't stamp identity (an out-of-tree module, or a very old caller that
hasn't been updated), it falls back to stamping its own plugin id. The wrapped call then arrives at the policy as
`CheckHelpers → <object>` — the legacy behaviour. Any rule that mentions `CheckHelpers` as a subject still applies.

---

## Configuration

All policy configuration lives under `/settings/permissions`. Two sub-trees:

### Global switches

```ini
[/settings/permissions]
; Master switch. When false (default), all calls are allowed.
; When true, the rules below form a strict allow-list - calls that
; don't match any rule are denied.
enabled = true

; Log every denial at warning level (default true).
log denials = true

; Log every allowed call at info level (default false - noisy).
log allows = false
```

There is no configurable "default for unmatched calls" — when the policy is enabled, the only meaningful stance is "no
match → deny."

### The rule table

```
[/settings/permissions/policies]
; NRPE may run anything in CheckHelpers and a few specific system checks.
NRPEServer = CheckHelpers.*, CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckDisk.check_drivesize

; A web user with read-only rights.
WEBServer:viewer = CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckHelpers.check_ok

; Web admins get everything.
WEBServer:admin = *

; The scheduler needs to run whatever you've scheduled it to run.
Scheduler = CheckHelpers.*, CheckSystem.*, CheckDisk.*
```

### Pattern syntax

- `*` matches any number of characters; `?` matches one.
- Subject `WEBServer` matches both `WEBServer` (no principal) and `WEBServer:<anything>`.
- Subject `WEBServer:` (trailing colon) matches only the no-principal form.
- Subject `WEBServer:*` matches any non-empty principal.
- Object `check_cpu` (bare command, no dot) matches the command across any owning module. Use `CheckSystem.check_cpu` to
  be explicit.
- Matching is case-insensitive.

---

## Step-by-step: turning it on securely

The safest way to roll out a permission policy is incremental: turn on audit-style logging first, write rules for what
you observe, then flip to enforcement once the rules cover the actual traffic.

### 1. Inventory the traffic first

Before writing any rules, find out who actually calls your agent and what they call. The easiest way is to turn the
policy *on* with a permissive rule and `log allows = true`, then watch the log:

```ini
[/settings/permissions]
enabled = true
log denials = true
log allows = true

[/settings/permissions/policies]
; Permissive bootstrap: lets everything through so production is not
; broken while we observe. We tighten this in step 3.
* = *
```

Restart service:

```
nscp service --restart
```

In `nsclient.log` you'll see entries like:

```
INFO  permissions: allowed WEBServer:admin -> CheckSystem.check_cpu
INFO  permissions: allowed NRPEServer -> CheckHelpers.check_drivesize
INFO  permissions: allowed Scheduler -> CheckSystem.check_uptime
```

Run for at least a full check cycle from your monitoring system. The collected subject/object pairs are the basis for
your real allow-list.

### 2. Write the real rules

From the log inventory, write one rule per subject. Be specific where you can:

```
[/settings/permissions/policies]
; Replace the wildcard rule with real ones.
NRPEServer = CheckHelpers.*, CheckSystem.check_cpu, CheckSystem.check_drivesize, CheckDisk.check_drivesize
Scheduler = CheckHelpers.*, CheckSystem.*, CheckDisk.*
WEBServer:admin  = *
WEBServer:viewer = CheckSystem.check_cpu, CheckSystem.check_drivesize
```

A few practical tips:

- Group your monitoring server's calls under a single subject
  (`NRPEServer`) unless you intend to set up principal-aware rules.
- Don't use `*` as a subject unless you really mean "anyone who can
  reach me." Specific subjects are easier to audit.
- For wrapper commands (`check_multi`, `check_always_ok`, aliases),
  remember the rule applies to the **caller** — see the CheckHelpers
  section above. You generally don't need a separate rule for
  CheckHelpers as a subject.

### 3. Switch from observe to enforce

Remove the catch-all `* = *` rule, leave `log denials = true` (default), and restart:

```
nscp service --restart
```

Watch the log for `permissions: denied ...` entries during the next check cycle. Each denial tells you a rule that's
missing — adjust and reload until the log is clean.

Optional: while iterating, leave `log allows = true` so you can confirm each check is hitting the rule you expect. Turn
it off once the rules are stable — allows fire on every call and can flood the log on busy agents.

### 4. Verify the proxy hops

If you use `CheckHelpers` wrappers (`check_multi`, `check_always_*`, aliases), confirm the wrapped calls are attributed
to the original caller and not to `CheckHelpers`. From an external caller, run a wrapper:

```
check_nrpe -H agent -c check_multi -a 'command=check_cpu' 'command=check_drivesize'
```

The log should show two `allowed` lines under the same subject (e.g. `NRPEServer`), one for the inbound `check_multi`
and one for the wrapped `check_cpu` / `check_drivesize`. If you instead see a second hop attributed to `CheckHelpers`,
something is wrong: either the caller isn't stamping identity (out-of-tree module) or you're running an older NSClient++
that pre-dates the forwarding behaviour.

### 5. Keep the policy file under version control

The policy file (`/settings/permissions/policies`) describes who can do what on the agent. Treat it the same way you'd
treat a firewall rule set — review changes, keep a history, deploy via your normal config management. A drop-in
`permissions.ini` referenced from the main `nsclient.ini` via `[/includes]` is convenient:

```ini
[/includes]
permissions = /etc/nsclient/permissions.ini
```

---

## Logging

| When                                     | Level | Example line                                                         |
|------------------------------------------|-------|----------------------------------------------------------------------|
| Call denied (default on)                 | ERROR | `permissions: denied WEBServer:viewer -> CheckSystem.check_eventlog` |
| Call allowed (off by default; opt-in)    | INFO  | `permissions: allowed NRPEServer -> CheckSystem.check_cpu`           |
| Policy reloaded                          | DEBUG | `permissions: loaded 4 rule(s), enabled=true`                        |
| Misconfiguration (e.g. settings missing) | ERROR | `permissions: failed to load: ...`                                   |

Restart the service:

```
nscp service --restart
```

That picks up changes to `/settings/permissions` and re-reads the `policies` table. Rules removed from the file are
dropped from the active table.

---

## Threat model

The policy layer defends against:

- A misconfigured remote server (e.g. NRPE exposing a check the operator didn't intend to expose).
- An accidentally-broad WEB role gaining access to a new check shipped in an update.
- A scheduled task drifting beyond what it was originally permitted to call.

It does **not** defend against a malicious in-process module — that adversary already has the same privileges as the
agent itself. The calling module's identity is stamped by the trusted `nscapi::core_helper` library, but a module that
bypasses `core_helper` entirely (by talking to the NSAPI C ABI directly) can stamp anything it wants. The model trusts
loaded DLLs; it gates which *commands* a given trusted caller may run.

---

## Frequently asked

**Q: Can I write deny rules?**
Not today. The current syntax is allow-only ("if any rule grants, allow; otherwise deny"). Deny prefixes (
`!CheckSystem.check_*`) are a natural future extension if a real use case arises.

**Q: Can I have per-argument policies (e.g. `check_drivesize` for `C:` but not `D:`)?**
No. Use a command alias under `[/settings/check helpers/alias]` to pre-bind the argument, then write a rule that grants
the alias name.

**Q: Why don't I see a denial in the log?**
Either `log denials = false`, or the call was *allowed* by a rule you didn't expect (run with `log allows = true` to
see). Check the inbound subject — a missing `nscp.caller_plugin_id` resolves to `*`, which matches any `*` subject rule.

**Q: My out-of-tree module can't be reached now that I've enabled policy. Why?**
If the module bypasses `nscapi::core_helper` and talks to NSAPI directly, it won't stamp identity, so its calls arrive
as subject `*`. Either update the module to use `core_helper`, or add a rule with subject `*` scoped to the specific
commands it needs.
