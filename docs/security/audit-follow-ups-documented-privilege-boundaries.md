---
title: "Audit follow-ups: administrator-equivalent grants, USER_WRITABLE_CONFIG, process command lines and the dead /metrics route"
fixed_in: next
severity: "Informational"
modules: [WEBServer, CheckSystem, CheckSystemUnix, packaging]
action: conditional
---
Five informational findings from the 2026-09 source review were closed. None
of them changes what a caller can reach; four put on record a boundary the
documentation did not state, and one removes a route that could not be
reached.

- **Web grants that amount to code execution were not named.** A role
  carrying `settings.put` can write an external script definition, open
  `allow arguments` and reload; `console.exec`, `modules.post`,
  `scripts.add.*`, `settings.delete`, `legacy` and `*` are in the same class.
  The [securing guide](../setup/securing.md#grants-that-equal-administrator-access)
  now lists them, and notes that `logs.put` can forge lines in the agent log.
- **The MSI property `USER_WRITABLE_CONFIG` gives the local `Users` group
  write access to `nsclient.ini`**, which lets any local user have a script of
  their choosing run as `SYSTEM`. The
  [MSI options](../setup/installing.md#msi-options) now document it, with that
  warning.
- **`check_process` returns full process command lines** through the
  `command_line` keyword and its `fetch-only` feed, read with the agent's
  privileges; command lines often carry passwords and tokens. The
  [data disclosure table](../setup/securing.md#data-disclosure-restricting-what-a-check-may-read)
  and the `check_process` reference now say so.
- **The REST guide's user example showed a clear-text stored password.**
  `nscp web add-user` stores a PBKDF2 hash; the example now shows that, and
  says that a password hand-written into `nsclient.ini` stays in clear text on
  disk until the command is run for that user.
- **The legacy `GET /metrics` route was registered but unreachable** - the
  static file handler claims every `/metrics` URL first - while the REST guide
  documented it as live. It is removed, and a test pins that the URL never
  answers with metrics, so a later change to the handler order cannot bring a
  `legacy`-grant route back.

**What to do:** if you installed with `USER_WRITABLE_CONFIG=1`, or give web
roles any of the listed grants, check that every account involved is one you
would trust as a local administrator.
