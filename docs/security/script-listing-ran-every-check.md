---
title: "Listing scripts over REST ran every check"
fixed_in: next
severity: "Low"
modules: [WEBServer, LUAScript, PythonScript]
action: none
---
`GET /api/v2/scripts/lua` and `GET /api/v2/scripts/py`, without `all=true`, list
the queries the agent serves. To build that list the script modules asked the
core for every query's parameters, and the core collects a query's parameters by
running it. So a caller holding only `scripts.lists.LUAScript` or
`scripts.lists.PythonScript` - grants for listing - could make the agent run every
check it has, once per request: every Lua and Python handler included, with
whatever side effects it has, and with arguments the handler did not expect. The
listing now asks for the names only, which is all it ever printed.

`GET /api/v2/queries/<name>/help` (grant `queries.list`) ran the one script
handler it names the same way. The script modules now answer that request
themselves - a script's query declares no parameters - without running the
handler.

**What to do:** nothing.
