---
icon: "🔒 📦"
modules: [NSCANgClient]
action: conditional
---
**Windows: the installer no longer ships `NSCANgClient`.** A few antivirus
engines report `NSCANgClient.dll` as malware with false positives, which made
the whole MSI look infected, so the module now ships in the Windows zip only.
An MSI upgrade removes the copy an earlier installer put there, and a service
configured with `NSCANgClient = enabled` then logs that the module was not
found. If you submit results over NSCA-NG from Windows, copy
`modules\NSCANgClient.dll` from the zip of the **same version and platform**
into the installation's `modules` folder after upgrading, and again after
every later upgrade: the installer does not own a file copied this way, so it
never replaces it. The steps are on the
[NSCANgClient reference](../reference/client/NSCANgClient.md) page. The Linux
and macOS packages are unchanged. See the
[security notice](../security/notices.md#nscangclient-antivirus-false-positives-and-the-module-leaves-the-windows-installer).
