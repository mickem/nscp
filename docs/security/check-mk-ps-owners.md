---
title: "check_process fetch-only and the Checkmk ps section now name each process's owner"
fixed_in: 0.27.0
severity: "Low"
modules: [CheckSystem, CheckSystemUnix, CheckMKServer]
action: conditional
---
`check_process fetch-only`, which builds the Checkmk `<<<ps>>>` section, now
fills the user field with the account that owns each process (`DOMAIN\name`
on Windows, the user name on Linux; the SID or numeric uid when the account
has no name). It used to leave the field empty. This resolves owners even
though `resolve-owner` is off by default for the check itself, so the account
names of every process the agent can see now leave the host:

* in the agent dump `CheckMKServer` serves to every address in its
  `allowed hosts`, and
* to any caller allowed to run `check_process` with arguments (NRPE with
  `allow arguments`, REST), who could already read every process's command
  line through `fetch-only`.

Resolving a name can also block: `LookupAccountSid` on domain or Azure AD
accounts, NSS on Linux hosts backed by LDAP or SSSD. Each name is cached after
its first lookup, so this costs once per account, not once per fetch.

**What to do:** nothing, unless account names on a host are sensitive. Then
keep `CheckMKServer`'s `allowed hosts` to the Checkmk site, and stop callers
from passing `fetch-only` to `check_process` as described under
[Data disclosure](../setup/securing.md#data-disclosure-restricting-what-a-check-may-read).
