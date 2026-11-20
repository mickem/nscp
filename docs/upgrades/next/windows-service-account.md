---
icon: "🔧"
modules: [core]
action: none
---
**Windows: the service can run as its own virtual account (experimental).** Nothing to do: the service still installs
and runs as `LocalSystem`. `nscp service --account virtual` switches an installed service to `NT SERVICE\<service name>`
with a reduced set of privileges, and `nscp service --install --account virtual` installs it that way;
`nscp service --account system` switches back. It needs the modern [layout](../concepts/file-layout.md) and is refused
on the legacy one, with the command to run first. The choice is recorded in `boot.ini` (`[service] account`).

- The account is added to *Performance Monitor Users*, *Performance Log Users* and *Event Log Readers*, and removed from
  them when switching back.
- Checks that need administrator rights report less or fail under this account: other accounts' processes in
  `check_process`, some services and scheduled tasks, Hyper-V, and external scripts that run as another user.
- The MSI does not know about the account yet: an upgrade or repair registers the service as `LocalSystem` again.
  Run `nscp service --account virtual` after it.
- The service logs the account and the privileges it runs with at start-up (`Running as ...`).
