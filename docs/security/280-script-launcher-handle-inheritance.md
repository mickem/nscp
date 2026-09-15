---
title: "External scripts inherited every inheritable handle of the service"
fixed_in: next
severity: "Medium"
modules: [CheckExternalScripts]
action: none
---
The script launcher created both stdio pipes with every end inheritable and
started the child with full handle inheritance, so a script received every
inheritable handle the service held at that moment. Checks run concurrently:
a script spawned while another was running inherited that other script's
stdout pipe — both ends — and could read its output or write a forged result
into it. Any other inheritable handle in the service crossed the same way.
A script run as a lower-privileged account through `user =` was exactly the
case this defeated: the sandbox that setting is meant to provide leaked the
service's handles into it. The Unix launcher had the smaller version of the
same gap: the pipe descriptors were not close-on-exec, so a script forked at
the wrong moment carried another script's write end, and every other
descriptor the service held (listener sockets, the log file) was the child's
to use.

On Windows only the two ends the child uses are now inheritable, the child is
started with a `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` naming exactly those two
handles, and on an OS without that API (the XP build) spawns are serialised so
that no two scripts' pipe ends are ever inheritable at the same time. On Unix
the pipe is created close-on-exec and the child closes every descriptor above
stderr before it runs the script.

**What to do:** nothing. A script that relied on an inherited handle it was
never meant to have (there is no supported way to obtain one) stops seeing it.
