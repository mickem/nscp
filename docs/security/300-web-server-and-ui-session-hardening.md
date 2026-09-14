---
title: "Web server and web UI: identity metadata, log buffer, logout and bundle staging"
fixed_in: next
severity: "Medium"
modules: [WEBServer]
action: none
---
Four findings from a review of the web server and the shipped web UI. None is
remotely exploitable without credentials except the second.

#### The legacy `/query.pb` route let a caller choose its own subject

The core permission layer decides what a request may run from the calling
module and principal, which it reads out of two metadata keys on the request
header (`nscp.caller_plugin_id`, `nscp.principal`). Those keys are stamped
in-process by `core_helper` and are trustworthy for a request that was built
inside the agent.

`POST /query.pb` forwarded the caller's protobuf into the core verbatim,
header included — so a caller who set the two keys picked its own subject and
satisfied any allow-list rule written for another module or user. The route now
parses the body and refuses, with `400`, any request that carries either key;
nothing legitimate sends them over HTTP. The `legacy` grant remains
RCE-equivalent on its own ([The `legacy` WEB permission is flagged and no longer seeded by
default](#the-legacy-web-permission-is-flagged-and-no-longer-seeded-by-default)), so this
matters where the policy system is used to constrain what legacy callers may
reach.

#### The in-memory log buffer had no upper bound

The module subscribes to every log line the agent produces and appended each to
a buffer that only an authenticated `DELETE /api/v2/logs` emptied. Every
rejected request — no credentials, a host outside `allowed hosts`, a bad token
— logs an error, and a request is rejected *before* it is authenticated, so
anyone who could reach the port could grow the buffer by one heap entry per
request for the life of the process.

It is now a ring of 1000 entries, the newest kept, matching the event store
next to it. The error tally the UI badge shows still counts every error the
agent reported, not just the ones still buffered.

#### `nscp web install-ui` staged the download in the shared temp directory

The command is documented to be run as root. It wrote the downloaded bundle to
`${temp}/nsclient-web-<version>.zip` — a world-writable directory, a name fully
predictable from the agent's own version — with a truncating open, then hashed
that file and extracted it. A local user could pre-create a symlink there and
have root truncate a file of their choosing, or pre-create a file they own and
rewrite its content between the hash check and the extraction, ending with
attacker-chosen HTML and JavaScript installed as the admin web UI.

The bundle is now verified in memory and written once, into a directory created
for that one install under the (root-owned) web path, with a random name and an
exclusive, symlink-refusing open — the same staging helper the REST script
upload uses ([REST script uploads were staged at a predictable
path](#rest-script-uploads-were-staged-at-a-predictable-path)). Nothing is written to `${temp}`
at all.

#### Logging out of the web UI did not revoke the session token

The UI's logout dropped the bearer token from its own state but never called
`DELETE /api/v2/login`, so the token stayed valid on the server for the
remainder of its eight-hour life. A copy taken from a shared or kiosk machine,
a browser profile backup or a proxy log kept working after the administrator
had logged out — with whatever role it carried, `full` included. Logout now
revokes the token server-side first, and clears the stored copy immediately
rather than on the next page load.

**What to do:** nothing required. Anyone who logged out of the web UI on a
machine they do not control should still assume the token was live until its
expiry, and change the password if it was captured.
