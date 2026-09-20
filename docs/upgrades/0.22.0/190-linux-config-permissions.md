---
icon: "🔒"
modules: [packaging]
action: conditional
---
**The DEB and RPM packages now install `nsclient.ini` and the log directory
owner-readable.** `/etc/nsclient` becomes `root:nsclient 0750`,
`/etc/nsclient/nsclient.ini` `root:nsclient 0640`, `/var/log/nsclient`
`nsclient:nsclient 0750` and the log file `0640`; they were world-readable, and
the configuration file holds the web admin password, the NRPE/NSCA passwords and
the submit-client tokens in plaintext. The post-install scripts apply this on
upgrade as well, so an existing host is fixed by upgrading. **If a local script
or a monitoring user reads either path without being root or in the `nsclient`
group, add it to that group first** — otherwise it will start getting permission
denied. Windows is unaffected. See the
[security notice](../security/notices.md#linux-packages-install-nsclientini-and-the-log-directory-owner-readable).
