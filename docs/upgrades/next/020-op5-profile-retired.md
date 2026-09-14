---
icon: "💥"
modules: [packaging, Op5Client]
action: conditional
---
**The op5 installer profile is gone; the op5 client is not.** With the page that offered it, the Windows installer
drops the `OP5Monitoring` feature, the `op5.ini` it laid beside `nsclient.ini`, and the four helper scripts it
installed into `scripts\op5`. `MONITORING_TOOL=OP5` no longer selects a baseline - it installs the generic one, the
same as `MONITORING_TOOL=GENERIC`.

Only installs that used that profile are affected, and only on their next upgrade:

- **Your configuration is not touched.** An upgrade neither removes settings you already have nor stops including
  `op5.ini` if your `nsclient.ini` includes it. What goes away is the copy of `op5.ini` the installer used to refresh,
  so keep your own copy of anything in it you still rely on - the installer will not reinstate it.
- **`ADDLOCAL=OP5Monitoring` fails.** A silent install naming the feature has to drop it; `ADDLOCAL=ALL` is unaffected.
- **The scripts under `scripts\op5` stop being installed.** Copy the ones you call from external-script definitions
  into `scripts\` before upgrading, or the definitions pointing at them break.
- **`Op5Client` still ships**, now under the *Various client plugins* feature, and `OP5_SERVER`, `OP5_USER` and
  `OP5_PASSWORD` still configure passive submission through op5's Northbound API.
