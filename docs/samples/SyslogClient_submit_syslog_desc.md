#### About `submit_syslog`

`submit_syslog` sends a check result to a **syslog** server as a BSD-syslog
(RFC 3164) message over UDP, port 514 by default.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=syslog`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying that the messages
arrive and are classified as intended.

##### Mapping a check status to a syslog severity

Syslog has no notion of OK/WARNING/CRITICAL, so each status is mapped to a
severity. The defaults are deliberately loud at the top end:

| Check status | Severity        |
|--------------|-----------------|
| OK           | `informational` |
| WARNING      | `warning`       |
| CRITICAL     | `critical`      |
| UNKNOWN      | `emergency`     |

`emergency` for UNKNOWN is worth changing on most installations —
on a traditional syslog host `emerg` is broadcast to every logged-in terminal,
and a check that goes UNKNOWN because a service is restarting will do that
repeatedly. `unknown-severity=warning` (or `error`) is usually the saner choice.

`facility` (default `kernel`) sets the facility the messages are filed under.
`kernel` is another default worth changing: it collides with actual kernel
messages, and most syslog daemons treat `local0`–`local7` as the range reserved
for application use.

##### Message shape

`tag template` (default `NSCA`) is the syslog tag, and the message body comes
from the `message_syntax` setting (`message template` on the command line),
default `%message%`.

**`%message%` is the only substitution either template performs.** `%source%`
and the other placeholders used elsewhere in NSClient++ are passed through
literally here, so a template of `%source%: %message%` emits the text
`%source%: ` followed by the check output. Put the check identity in the tag, or
in the check's own message, rather than in the template.

Keep the tag short and stable — many syslog daemons and downstream parsers key
on it. Note that the default tag is `NSCA`, which is misleading in a log that
also carries real NSCA traffic; setting it to something like `nscp` is worth
doing.

##### Transport

This is **plain UDP**: no encryption, no authentication and no delivery
guarantee. Messages can be dropped silently by any hop, they are readable by
anyone on the path, and anyone who can reach the port can forge them. There is
no TLS (RFC 5425) or TCP transport option here.

Use it inside a trusted network segment, and do not treat the receiving log as
evidence that a check actually ran — a dropped datagram is indistinguishable
from a check that never fired. For anything that has to be reliable, submit
through a transport that acknowledges, such as
[NSCA-ng](NSCANgClient.md) or [NRDP](NRDPClient.md).
