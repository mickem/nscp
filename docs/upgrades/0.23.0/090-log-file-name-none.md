---
icon: "🔧"
modules: [core]
action: conditional
---
**`file name = none` now really switches the log off on Windows.** Nothing to do
unless `[/settings/log] file name` is set to `none`. Unix installations already
behaved correctly.

`none` is documented as "no log file", but the name was joined to the
installation directory before the sentinel was tested. On Windows, where that
directory is not empty, `none` became a real file called
`C:\Program Files\NSClient++none` and file logging stayed on. On unix the join
contributed nothing, so the sentinel survived and the setting worked.

If this applied to you, the stray file is left where it is — delete it once you
have checked you do not need its contents.

`none` is now recognised by the path expander itself, so it is equally safe in
every setting that accepts it, including every `ca` option where it means "use
the TLS library's own trust store".

Where a bare log **file name** ends up is a separate change with its own note —
see the one about relative paths being taken relative to the folder their
setting owns, which covers the log file on both platforms.
