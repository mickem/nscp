## Service facts

Enable `services.installed = true` under `[/settings/system/windows/facts]`
or `[/settings/system/unix/facts]` to inventory local services, including stopped
and disabled services. Records contain the service name, display name and native
startup type. Linux requires systemd and includes installed templates and loaded
instances. Collection starts on the first scheduled facts round or a manual
`facts refresh`; it does not delay agent startup. See [Host Facts](../../concepts/facts.md#servicesinstalled)
for fields, limits and failure behavior. The setting defaults to false. This
inventory is not yet supported on macOS.
