---
icon: "🔧"
modules: [core]
action: none
---
**Remote `[/includes]` and `[/attachments]` are now refreshed on their own.**
An agent configured from an `http(s)://` settings url re-downloads its whole
configuration every *settings maintenance interval* (`/settings/core`
`settings maintenance interval`, default `5m`). Until now that pass stopped at
the top-level file: a file pulled in by `[/includes]`, and every
`[/attachments]` target, was only re-fetched when the top-level file itself
happened to change — so on a server where `nsclient.ini` is the stable part and
the included file is the one that moves, the include stayed pinned to whatever
it held when the agent started. Both are now fetched on every pass, and a
change in either triggers the same reload a change in the top-level file does.

Nothing to configure. Two things to be aware of:

* The settings server now sees one request per included file and per
  attachment on every interval, not just one for the top-level file. The
  responses are hash-compared, so an unchanged file still costs nothing beyond
  the request itself — but if you serve a large attachment to a large fleet,
  size the interval accordingly.
* A configuration change made only in an included file now takes effect within
  one interval instead of requiring a service restart. If you were touching
  the top-level file to force includes through, that workaround is no longer
  needed.
