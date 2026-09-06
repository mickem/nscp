All the examples below were sent to a listener on `127.0.0.1:5514`; the second
block of each pair is the datagram that arrived.

**Submit a single result:**

```
submit_syslog host=127.0.0.1 port=514 command=check_disk result=WARNING "message=/var is 91% full"
OK: Data presumably sent successfully
```

```
<4>Sep  4 13:01:06 vm NSCA /var is 91% full
```

Priority `4` is facility `kernel` (0) x 8 + severity `warning` (4) — the
defaults. Note the tag: it is `NSCA` out of the box, which is misleading in a
log that also carries real NSCA traffic.

**Use a sensible facility and tag:**

`local0`-`local7` is the range syslog reserves for application use; `kernel`
collides with actual kernel messages.

```
submit_syslog host=127.0.0.1 port=514 command=check_disk result=WARNING "message=/var is 91% full" facility=local0 "tag template=nscp"
OK: Data presumably sent successfully
```

```
<132>Sep  4 13:01:43 vm nscp /var is 91% full
```

**How each status maps to a severity:**

```
submit_syslog ... result=CRITICAL "message=/var is full" facility=local0
<130>Sep  4 13:01:07 vm NSCA /var is full          # local0.crit

submit_syslog ... result=OK "message=all good" facility=local0
<134>Sep  4 13:01:08 vm NSCA all good              # local0.info

submit_syslog ... result=UNKNOWN "message=no data" facility=local0
<128>Sep  4 13:01:09 vm NSCA no data               # local0.emerg
```

That last one is the default worth changing: on a traditional syslog host
`emerg` is broadcast to every logged-in terminal, and a check that flaps into
UNKNOWN will do that repeatedly.

```
submit_syslog ... result=UNKNOWN "message=no data" facility=local0 unknown-severity=warning
<132>Sep  4 13:01:10 vm NSCA no data               # local0.warning
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_syslog host=127.0.0.1 port=514 "batch=check_a|OK|first" "batch=check_b|CRITICAL|second" facility=local0
OK: Data presumably sent successfully
```

```
<134>Sep  4 13:01:24 vm NSCA first
<130>Sep  4 13:01:24 vm NSCA second
```

Note that the record separator is `|` by default, not a comma. A malformed batch
is not rejected — `batch=check_a|OK|first,check_b|CRITICAL|second` is read as one
record whose message is `first,check_b`, and the rest is silently dropped.

**`%message%` is the only substitution in the message template:**

```
submit_syslog ... "message template=%source%: %message%" facility=local0
<132>Sep  4 13:01:44 vm nscp %source%: /var is 91% full
```

**"Sent successfully" is not delivery:**

The transport is plain UDP, so the OK means the datagram was handed to the
socket — nothing more. A dropped datagram is indistinguishable from a check that
never ran, which is why this is a poor choice for anything that has to be
reliable.

```
submit_syslog host=192.0.2.1 port=514 command=check_ok result=OK "message=nobody is listening"
OK: Data presumably sent successfully
```
