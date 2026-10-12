---
icon: "🔧"
modules: [core, WEBServer]
action: conditional
---
**The agent now reports configuration it ignores, and `nscp settings --validate` exits with 1 when it finds any.**
Nothing to do unless a script runs `nscp settings --validate`. A section written without its leading
`/` (`[settings/external scripts]`), a misspelled section or key, or a key above the first section is
never read, and used to be ignored without a word - a script defined in such a section was simply
`Unknown command` (#647). The agent now logs a warning naming each one at startup and on every reload,
the web UI shows a *Configuration problems* banner (and a *Problems* button on the Settings page), and
`GET /api/v2/settings/problems` returns the list. Sections and keys that only a module which is not
loaded would use are listed as info and raise nothing. `nscp settings --validate` prints the same
report and now exits with `1` when it contains a warning; it used to exit with `0` whatever it found,
and never reported a key at all. Add `--load-all` to have every installed module register its
settings first, so anything still unclaimed is reported as a warning too.

The check found one of its own: the core registered its section as `settings` rather than `/settings`,
so `nscp settings --update` wrote an empty `[settings]` into every file it touched. It no longer does;
an existing empty `[settings]` is harmless, listed as info, and can be deleted.
