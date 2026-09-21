---
icon: "🔧"
modules: [core]
action: conditional
---
**An unrecognised `${...}` path token is now an error, and a `[paths]` override
has to name an absolute location.** Nothing to do if your paths are spelled
correctly and your overrides are absolute — which is the normal case. If not,
the agent now says so in the log instead of quietly using the wrong folder.

Two related changes:

* **An unknown token is reported and the setting that carried it is skipped.**
  It used to resolve to the installation directory, so a mistyped
  `${scripst}/check.bat` was not rejected but turned into a real path under the
  install folder — and whatever depended on it went somewhere nobody was
  looking. This is the same silent failure that made a pre-0.17 `${host}` in a
  path produce a mangled name rather than an error. The tokens are still open
  ended: anything you define in `boot.ini`'s `[paths]` section counts as known.
  Note that `${appdata}` and `${common-appdata}` are Windows-only and are now
  rejected on unix, where they previously resolved to the install directory.
* **A `[paths]` or `--path-override` entry that does not resolve to an absolute
  path is ignored, with an error naming it, and the built-in default applies.**
  An override may still be written in terms of other tokens
  (`scripts = ${shared-path}/mine`); it is the resolved value that has to be
  absolute. A relative override would be read and written relative to the
  service's working directory — `C:\Windows\System32` for a Windows service,
  `/` under a bare init script, the package directory under the shipped systemd
  unit — so what it meant depended on how the agent happened to be started.
  A `--path-override` that is already unusable is dropped before `boot.ini` is
  opened, so the configuration and the folders the agent writes into can never
  come from different trees; one written in terms of a `[paths]` token is
  judged once that section has been read.
* **A token that refers to itself is rejected too.** `boot-conf =
  ${boot-conf}/boot.ini` used to expand into a growing `/boot.ini/boot.ini/…`
  that looked absolute enough to be accepted. A cycle is now reported like any
  other bad token, and the built-in default applies.

If you scripted `--path-override log-path=.` or similar against a build tree,
give it an absolute path instead.

Check the log after upgrading: every rejected token and override is reported
with the key that carried it.
