---
icon: "🔒"
modules: [WEBServer, LUAScript, PythonScript]
action: none
---
**Listing scripts over REST no longer runs every check.** Nothing to do. `GET
/api/v2/scripts/lua` and `GET /api/v2/scripts/py` (and `nscp lua list --query` / `nscp py list
--query` behind them) asked the core for each query's parameters, which it collects by running the
query - so listing the scripts ran every check on the agent once, each Lua and Python handler
included, side effects and all. They now ask for the names only. `GET /api/v2/queries/<name>/help`
for a script's query no longer runs the handler either. See the
[security notice](../security/notices.md#listing-scripts-over-rest-ran-every-check).
