---
title: "script root: a service-writable folder can hold the scripts the service runs"
fixed_in: 0.26.0
severity: "Low (opt-in; no change to a default install)"
modules: [CheckExternalScripts, PythonScript, LUAScript, CheckMKClient, CheckMKServer]
action: conditional
---
The new `script root` setting lets the script modules load their scripts from
a folder other than `${scripts}` - most usefully `fleet`, the tree under
`${fleet-folder}` where the fleet sync stages a bundle's `scripts/` entries.
That tree is, by design, writable by the service account, and so is any other
folder an operator might choose: `${shared-path}` on the modern Windows layout
is in `%ProgramData%`.

With the root pointed at such a folder, write access to it is execution as
the service account: an external script registered as `scripts\check.bat` is
whatever file sits at that path, and a Python or Lua helper in `lib` is
whatever file carries that name. That is the same property
`CheckExternalScripts`' `script path` option has always had, and on a fleet
host it is what a signed bundle is meant to do; the sync verifies every bundle
before a byte of it is staged, and a local change to the tree is reverted on
the next apply and reported to the server as a local override. What the
setting adds is the reach: a folder the service can write to is now also a
folder it executes from.

The show/delete sandbox of `ext-scr`, `nscp py` and `nscp lua`, and of the
`/api/v2/scripts` routes, moves with the root: those operations are confined
to the configured folder as they were to `${scripts}`, with symlinks
resolved, so a name cannot reach above the tree. `ext-scr add --import` keeps requiring its
source to be inside the script folder, `${shared-path}` or the upload staging
area. A root that cannot be entered fails the check out loud rather than
running the script from wherever the service happened to be started.

**What to do:** nothing on an install that does not set `script root`. Where
you set it, restrict write access to the folder the way you would restrict
`${scripts}` itself - the fleet folder is already owned by the service
account and nothing else - and prefer `fleet` over a hand-managed folder, so
that what runs is what a signed bundle delivered.
