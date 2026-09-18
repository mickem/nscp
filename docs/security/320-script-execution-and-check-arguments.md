---
title: "Script execution and check arguments: NUL truncation, import sandbox, pipe reads, handle leak, docker endpoint, remote-connection checks"
fixed_in: next
severity: "Low"
modules: [CheckExternalScripts, CheckDocker, CheckWMI, CheckMySQL, CheckMSSQL, docs]
action: conditional
---
Five findings from a review of the script launcher and the checks that take a
connection or a path as an argument.

#### A NUL in an argument truncated the Windows command line

Neither metacharacter filter contained NUL, protobuf strings carry it and REST
forwards URL parameters verbatim. `CreateProcessW` reads `lpCommandLine` as a C
string, so with a template of `check.exe $ARG1$ --read-only` an `$ARG1$` of
`x\0` ran `check.exe x`: every operator-fixed argument after the substitution
point silently disappeared. On unix only the one argv element was truncated,
which is still not what the template says.

A NUL in an argument is now refused whatever `allow nasty characters` says. It
is not a metacharacter an operator can decide to allow — it changes what the
launcher was asked to run.

#### `ext-scr add --import` read any file the agent could

[Notice 050](notices.md#clients-scripts-and-filter-framework-hardening) confined
`show` and `delete` to the script root so an administrator could not read files
outside it. `add --import` copied from any path the service account could read
*into* the script root under a chosen name, after which `show` returned the
bytes — so the sandbox held only until someone carried a file inside it.
Reachable from the `nscp` command line and over REST through the legacy `/exec`
route.

Import sources are now confined to the script root, `${shared-path}` and the
upload staging area under `${temp}`, which are the three places a script is
legitimately imported from. `PUT /api/v2/scripts` was never affected; it pins
`--import` to its own staged upload.

#### A script printing in exact buffer-sized chunks wedged a worker (Windows)

The read loop kept calling `ReadFile` as long as the previous read had filled
the whole chunk, and `ReadFile` on a pipe blocks until a byte arrives. A script
that wrote exact multiples of 4086 bytes and then stalled parked the worker
thread with no deadline check at all, past the wall-clock timeout added in
notice 050, until the child wrote again or exited. Each read now takes only what
`PeekNamedPipe` reported, which cannot block.

#### A timed-out script leaked a process handle (Windows)

The timeout branch returned before the `CloseHandle`, so every timed-out — and
every forked — invocation leaked a handle and kept the process object alive in a
service that runs for months. A caller able to make a script hang through its
arguments drove this remotely. Both handles are owned by an RAII wrapper now.

#### `check_docker host=` probed any local socket or pipe

The endpoint validator already refused a UNC path, so a rogue remote pipe server
could not be reached. What remained was that the agent would `GET
/containers/json` against any absolute unix socket path or `\\.\pipe\<name>` a
caller named, and report the status code or parse error: a read-only probe of
every socket and pipe on the host, performed as `SYSTEM` or `root`. Which daemon
the agent talks to is now settled by `endpoint` under `[/settings/docker]`; a
request repeating that value is still accepted, one replacing it is not.

#### Checks that connect somewhere else are documented as such

`check_wmi`, `check_mysql`, `check_mssql` and `check_uncpath` take the host, the
credentials and sometimes the whole connection string as arguments. Where the
caller supplies those, they are server-side request forgery from the agent's
network position, and an outbound authentication attempt made on the caller's
behalf. That is by design and the controls are the ones that decide whether a
caller may pass arguments at all — now stated as such in the securing guide and
in [Restricting what a check may read](../concepts/check-access.md). `check_wmi`
remains the only one with a named-target gate of its own.

**What to do:** nothing on a default install. If a check definition passes
`host=` to a docker check, make sure it names the same endpoint as
`[/settings/docker]`, or drop the argument. If any workflow relies on
`ext-scr add --import` reading from outside the script folder, copy the file
into `${shared-path}` first.
