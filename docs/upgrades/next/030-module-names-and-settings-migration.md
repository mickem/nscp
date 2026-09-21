---
icon: "🔒"
modules: [core]
action: conditional
---
**A module is named by a file name, and settings migration stays local.** A
`[/modules]` entry (or a `Control.LOAD` registry request) whose right-hand side
is a path rather than a single file name is refused with an error in the log: an
absolute value used to replace the module path outright and `..` walked out of
it, which meant naming a module was also naming any file on the host. The
`./modules` fallback now resolves against `${exe-path}` instead of the process's
current directory. `settings --load` / `--save` against an
`http://` or `https://` context is refused; a remote settings source belongs in
`boot.ini`. Nothing to do unless a `[/modules]` entry points outside the module
path, in which case move the module there and name it.
