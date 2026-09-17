---
icon: "🆕"
modules: [NCPAServer]
action: none
---
**New module: an NCPA server on port 5693.** Nothing changes for existing
installs — `NCPAServer` is not enabled by default, and an agent that is upgraded
without enabling it listens on nothing new. Enable it to let Nagios Core and
Nagios XI poll this agent with the stock `check_ncpa.py` plugin and the XI NCPA
wizard, unmodified:

```ini
[/modules]
NCPAServer = enabled

[/settings/NCPA/server]
token = a-long-random-shared-secret
allowed hosts = 192.168.0.10
```

Two things are worth knowing before enabling it:

* **A token is required.** There is no default and none is generated. Until
  `token` is set the server answers every request with NCPA's
  `Incorrect credentials given.`, so a module enabled but not configured exposes
  nothing — but it also answers no checks.
* **TLS is required unless you opt out.** The server refuses to start without a
  certificate rather than putting the token on the wire in clear on every check.
  It defaults to the same `${certificate-path}/certificate.pem` the WEB server
  uses, so on a host that already runs the web server there is nothing to
  provide; `allow insecure = true` accepts cleartext explicitly.

On Windows the module ships in its own **NCPA support** installer feature, so an
existing installation needs the installer re-run with that feature selected. On
Linux the packages include it automatically.

See the [Active Monitoring with NCPA](../scenarios/ncpa.md) scenario for the
Nagios Core command definitions and the XI wizard walkthrough.
