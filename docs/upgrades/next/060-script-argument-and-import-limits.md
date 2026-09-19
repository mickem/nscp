---
icon: "🔒"
modules: [CheckExternalScripts]
action: conditional
---
**A NUL in a script argument is refused, and `ext-scr add --import` reads only
from the script folders.** `CreateProcessW` reads the command line as a C
string, so an argument containing a NUL truncated it and silently dropped every
operator-fixed argument after the substitution point; that is refused now
whatever `allow nasty characters` says, because it changes what the launcher was
asked to run rather than what the script sees. `add --import` copied from any
path the service account could read into the script folder, where `show` then
returned the bytes — which made the show/delete sandbox hold only until someone
carried a file inside it. Import sources are confined to the script root,
`${shared-path}` and the upload staging area; copy a script into one of those
first if a workflow relied on importing from elsewhere. `PUT /api/v2/scripts` is
unaffected. On Windows, a script that prints in exact buffer-sized chunks no
longer parks a worker thread past the timeout, and a timed-out or forked script
no longer leaks a process handle.
See the [security notice](../security/notices.md#script-execution-and-check-arguments-nul-truncation-import-sandbox-pipe-reads-handle-leak-docker-endpoint-remote-connection-checks).
