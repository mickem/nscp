---
icon: "🛰️"
modules: [packaging]
action: conditional
---
**A fleet install no longer writes local configuration.**
An MSI install with `FLEET_SERVER` used to apply the monitoring-tool profile as
well: it enabled NRPEServer, WEBServer and the check modules, set
`allowed hosts = 127.0.0.1`, generated a password and wrote the NRPE preset
into `nsclient.ini` - and because a local key wins over the fleet include,
those values silently overrode what the fleet server sent. Now the only thing
the installer adds is `[/includes] fleet = ${fleet-folder}/fleet.ini`, and the
configuration properties (`ALLOWED_HOSTS`, `NSCLIENT_PWD`, `CONF_SET`,
`CONF_INCLUDES`, `NRPEMODE`, `CERTIFICATE_KEY`, `NSCA_*`, `OP5_*`) are ignored
with a warning in the install log. An interactive install with `FLEET_SERVER`
on the command line no longer shows the monitoring-tool and configuration
pages. If your fleet deployment command passes any
of these, move the values to the fleet server; a fleet server that relied on
the profile's modules being enabled must now enable them itself. Hosts that are
already enrolled keep their existing `nsclient.ini` unchanged.
