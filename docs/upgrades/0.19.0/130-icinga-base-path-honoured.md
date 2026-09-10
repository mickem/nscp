---
icon: "🔧"
modules: [IcingaClient]
action: conditional
---
**An Icinga target address with a path prefix is now honoured.** The path of a
target `address` (`https://proxy.example.com/icinga/`) was parsed and then
dropped: every call went to `/v1/...` on the host, so an Icinga 2 master
published under a reverse-proxy subpath could not be reached and requests
landed on a path the operator did not intend. The prefix is now prepended to
every API path (`/icinga/v1/actions/process-check-result`). Addresses without a
path — the normal `https://icinga.example.com:5665` form, with or without a
trailing `/` — are unaffected. If a target address carries a path that is *not*
a subpath of the API (a leftover from another client's configuration, say),
remove it: it is now sent.
