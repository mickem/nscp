---
icon: "💥"
modules: [packaging, Op5Client]
action: conditional
---
**The op5 installer profile is gone; the op5 client is not.** With the page that offered it, the Windows installer
drops the `OP5Monitoring` feature, the `op5.ini` it laid beside `nsclient.ini`, and four helper scripts.
`MONITORING_TOOL=OP5` no longer selects a baseline - it installs the generic one, the same as
`MONITORING_TOOL=GENERIC`.

Only installs that used that profile are affected, and only on their next upgrade:

- **Your configuration is not touched.** An upgrade neither removes settings you already have nor stops including
  `op5.ini` if your `nsclient.ini` includes it. What goes away is the copy of `op5.ini` the installer used to refresh,
  so keep your own copy of anything in it you still rely on - the installer will not reinstate it.
- **`ADDLOCAL=OP5Monitoring` fails.** A silent install naming the feature has to drop it; `ADDLOCAL=ALL` is unaffected.
- **Four scripts stop being installed:** `check_ad.vbs`, `check_time.vbs`, `restart_service.ps1` and `services.vbs`.
  Despite the name of the folder they were built from, they installed straight into `scripts\`, beside the ordinary
  ones - so check your external-script definitions for those four names, and keep your own copies before upgrading if
  you still call them. An upgrade does not delete copies you put there yourself.
- **`Op5Client` still ships**, now under the *Various client plugins* feature. Configure it in `nsclient.ini` under
  `[/settings/op5]`, or with `nscp op5 install`. The `OP5_SERVER`, `OP5_USER` and `OP5_PASSWORD` MSI properties do
  **not** configure it and never have: the installer reads them from a place nothing fills in, so they have always
  been silently ignored. That is unchanged here, and unrelated to the profile being retired.
