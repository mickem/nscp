The examples below were sent to a Carbon listener on `127.0.0.1:2003`, with the
target configured as:

```ini
[/settings/graphite/client/targets/default]
address = 127.0.0.1:2003
path = nsclient.${hostname}.${check_alias}.${perf_alias}
status path = nsclient.${hostname}.${check_alias}.status
send perfdata = true
send status = true
```

**Submit a result directly (useful for testing the connection):**

```
submit_graphite host=127.0.0.1 port=2003 command=check_disk result=WARNING "message=/var is 91% full"
OK: Data presumably sent successfully
```

What arrives on the wire:

```
nsclient.vm..status 1 1788526945
```

Two things to notice. Only the status arrived — the submission carried no
performance data, and Graphite stores numbers, so there was nothing else to
send. And `${check_alias}` is **empty**, leaving a `..` in the path: neither
`command=` nor `alias=` populates it on this path.

**Route a real check to the target instead — which is how it is meant to be used:**

```
check_and_forward command=check_drivesize channel=GRAPHITE alias=drivesize
OK: Message submitted: GRAPHITE
```

```
nsclient.vm.drivesize./_used 9039142912 1788526958
nsclient.vm.drivesize./_used_percent 3 1788526958
nsclient.vm.drivesize./opt/claude-code_used 212594688 1788526958
nsclient.vm.drivesize./opt/claude-code_used_percent 88 1788526958
nsclient.vm.drivesize./opt/env-runner_used 31223808 1788526958
nsclient.vm.drivesize./opt/env-runner_used_percent 64 1788526958
nsclient.vm.drivesize.status 1 1788526958
```

Now `${check_alias}` is the alias the result was forwarded under, and every
performance counter becomes its own series.

**Watch out for separators in counter names:**

`${perf_alias}` is inserted verbatim, so the mount point `/opt/claude-code`
becomes `.../opt/claude-code_used` — a **dotted path split into extra levels**
by Graphite, and a whisper file created for each one. On Windows the same
happens with drive letters and colons.

Get the path template and the check's `perf-syntax` right before pointing a
fleet at it: Carbon creates a whisper file per distinct path on first write, so
anything volatile in the name has to be cleaned up by hand afterwards.

**Send only the numbers, not the status:**

Graphite renders a `0/1/2/3` status series poorly compared to a real alerting
system, so most installations turn it off:

```ini
[/settings/graphite/client/targets/default]
send status = false
```

**"Sent successfully" means handed to the socket:**

Carbon's plaintext protocol acknowledges nothing, so an OK here does not mean
the metrics were stored — only that the connection was accepted.
