---
icon: "🔧"
modules: [LUAScript, PythonScript]
action: none
---
**`nscp lua list` and `nscp py list` list the scripts on Linux packages.** Nothing to do. Both left out
every file whose path contained `lib` anywhere, meant for helper folders - and on a DEB or RPM install
`${scripts}` is `/usr/lib/nsclient/scripts`, so the listing, and `GET /api/v2/scripts/lua?all=true` /
`/api/v2/scripts/py?all=true` behind it, was always empty. Only a folder named `lib` below
`${scripts}/lua` or `${scripts}/python` is left out now, and `--include-lib`, which was accepted but
did nothing, lists those helpers too.
