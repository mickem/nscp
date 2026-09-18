---
icon: "🔒"
modules: [CheckExternalScripts]
action: none
---
**External scripts no longer inherit the service's handles.** A script now
receives only its own stdin and stdout/stderr pipe ends: on Windows through an
explicit inherit list (spawns are serialised on the XP build, which has no
such list), on Unix through close-on-exec pipes and a close of every
descriptor above stderr in the child. Previously a concurrently running
script's output pipe, and every other inheritable handle of the service,
crossed into each child (see the
[security notice](../security/notices.md#external-scripts-inherited-every-inheritable-handle-of-the-service)).
Nothing to do.
