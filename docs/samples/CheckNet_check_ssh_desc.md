#### About `check_ssh`

`check_ssh` confirms that an SSH server is reachable and presents a valid SSH
protocol banner. It connects to the port (default **22**), reads the greeting
the server sends on connect, and requires it to start with `SSH-` (e.g.
`SSH-2.0-OpenSSH_9.6`). Nothing is written to the peer, so it does not initiate
a key exchange or authenticate — it is a lightweight "is sshd up and answering"
probe.

It builds on [`check_tcp`](#check_tcp) (the `service=ssh` preset), so it shares
`check_tcp`'s keywords (`host`, `port`, `time`, `result`, `response`,
`connected`) and thresholds; on an SSH check `response` holds the banner the
server returned.

Default thresholds: **warning** `time > 1000`, **critical**
`time > 5000 or result != 'ok'`. A port that answers but is not SSH yields
`result = no_match` (CRITICAL); a closed port yields `result = refused`.

#### The parsed identification string

On top of those, `check_ssh` splits the SSH identification string
(RFC 4253 §4.2) into its parts, so the server's protocol and software version
can be thresholded directly instead of regex-matching the raw `response`:

```
SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5
    │   │             └── comments
    │   └── version ─────── software "OpenSSH" + software_version "9.6p1"
    └── protocol
```

`banner` keeps the raw identification line, and `protocol_major` /
`protocol_minor` expose the protocol version as numbers (`2` and `0` for
`2.0`; `99` is the minor for `1.99`).

`software` / `software_version` are split on the last `_` that is followed by a
digit, which keeps multi-word names intact (`OpenSSH_for_Windows_9.5` →
`OpenSSH_for_Windows` + `9.5`). A server that publishes an opaque build id
rather than a version (e.g. `SSH-2.0-GitLab-SSHD`) keeps the whole string as
`software` and leaves `software_version` empty; `version` always holds the full
field, so it is the safe one to regex against.

All of these are **empty** (and the numeric ones `0`) when no banner was read —
a refused or timed-out connection, or a port that is not speaking SSH. Since
the default critical already covers `result != 'ok'`, that case is caught
regardless; guard on `result = 'ok'` explicitly if you add your own thresholds
and want to keep the two failure modes apart.

A note on `protocol`: `1.99` is not "older than 2.0" — it means the server
speaks 2.0 *and* still accepts the insecure SSHv1, which is exactly what
`protocol_major < 2` is for.
