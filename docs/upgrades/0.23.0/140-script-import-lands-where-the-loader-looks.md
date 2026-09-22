---
icon: "🐛"
modules: [CheckExternalScripts, PythonScript, LUAScript]
action: none
---
**`ext-scr`, `py` and `lua` `add --import` now work on a fresh install, and
what they record is a path that resolves.** Nothing to change: existing
configuration is read exactly as before, and this only affects what the import
writes from now on.

Three things were wrong with importing a script, and each of them was silent
in its own way:

| Symptom | Was | Now |
| --- | --- | --- |
| Import fails outright | `copy_file` never created the destination folder, so the import died with a bare *No such file or directory* naming `…/scripts/python` | the folder is created |
| Imported command exits 127 on Linux | `ext-scr` recorded `scripts\<name>` — a Windows spelling of a path this module never resolves anyway | the destination's absolute path is recorded |
| `add --import` with no `--script` | the destination collapsed onto the script folder itself and the copy overwrote that path with a file | refused, naming the missing option |

`${scripts}/python` and `${scripts}/lua` only exist on Windows when the sample
scripts feature is installed, and a `${scripts}` override points wherever an
operator chose, so "import into a folder that is not there" was the normal
case, not the corner case. The Web UI's script upload goes through the same
`add --import`, so it failed the same way.

The command `ext-scr add --import` writes on Linux is now absolute:

```ini
[/settings/external scripts/scripts]
imported = /usr/lib/nsclient/scripts/imported.sh
```

That is not a style preference. `CheckExternalScripts` hands the value to the
shell verbatim — no `${...}` expansion, no search of the script folder — and on
Linux the launcher does not set a working directory for the child at all, so
only an absolute path can work. On Windows the child does start in
`${base-path}` with `${scripts}` directly below it, so the relative
`scripts\<name>` still resolves and is still what gets recorded there.

`ext-scr list`, `py list` and `lua list` had a matching defect: a path that did
not begin with `${base-path}` still had its leading separator sliced off,
producing a rootless `usr/lib/nsclient/scripts/check_x.sh` that named no file.
Those entries are what the Web UI's script list shows and what `show` is called
back with, so they now stay absolute when they are not below the install base.
