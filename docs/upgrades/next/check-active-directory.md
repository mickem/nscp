---
icon: "🧪 🏷️"
modules: [CheckActiveDirectory]
action: none
---
**New `CheckActiveDirectory` module (experimental).** Nothing to do on an
upgrade: the module is not enabled by default. On Windows,
`CheckActiveDirectory = enabled` under `[/modules]` adds
`check_ad_replication` (inbound replication links on a domain controller:
consecutive failures and last success), `check_secure_channel` (the
machine-account secure channel, verified through netlogon like
`Test-ComputerSecureChannel`) and `check_kdc` (a real Kerberos AS-REQ against
each KDC, so a KDC that accepts connections but no longer issues tickets is
caught). The module is marked experimental: its options, keywords and output
may still change. See the
[Active Directory & Identity](../scenarios/active-directory.md) scenario.
