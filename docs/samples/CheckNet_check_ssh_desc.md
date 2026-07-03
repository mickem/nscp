#### About `check_ssh`

`check_ssh` confirms that an SSH server is reachable and presents a valid SSH
protocol banner. It connects to the port (default **22**), reads the greeting
the server sends on connect, and requires it to start with `SSH-` (e.g.
`SSH-2.0-OpenSSH_9.6`). Nothing is written to the peer, so it does not initiate
a key exchange or authenticate — it is a lightweight "is sshd up and answering"
probe.

It is a thin preset over [`check_tcp`](#check_tcp) (`service=ssh`), so it shares
`check_tcp`'s keywords and thresholds:

| Keyword     | Description                                             |
|-------------|--------------------------------------------------------|
| `host`      | Host the check connected to                            |
| `port`      | Port the check connected to (default 22)               |
| `time`      | Connection + banner-read time in milliseconds          |
| `result`    | `ok`, `no_match`, `refused`, `timeout`, `resolve_failed`, … |
| `response`  | The banner the server returned                          |
| `connected` | `1` when the TCP connection succeeded                   |

Default thresholds: **warning** `time > 1000`, **critical**
`time > 5000 or result != 'ok'`. A port that answers but is not SSH yields
`result = no_match` (CRITICAL); a closed port yields `result = refused`.
