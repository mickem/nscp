Checks a RADIUS server over UDP using a shared secret. The command is experimental
and requires OpenSSL. Each invocation sends one request and uses a fresh random
identifier and request authenticator. `time` includes name resolution and the
exchange; `timeout` is in milliseconds. With several resolved addresses, the first
address in the selected `address-family` is tested. Test each NPS node separately.

#### Choose what the probe proves

- `mode=auth` sends PAP credentials and requires Access-Accept. Supply a dedicated
  test `username` and `password-file`. This tests that identity and policy; it does
  not test PEAP, EAP-TLS, certificate negotiation, or an interactive MFA exchange.
- `mode=reject` sends a fictional username (default `nsclient-radius-probe`) with
  an empty PAP password and requires Access-Reject. This demonstrates server
  responsiveness, not successful authentication. An unexpected accept is critical.
- `mode=status` sends RFC 5997 Status-Server without credentials. Enable this only
  for servers confirmed to support it. It tests that server, not a downstream
  realm or proxy chain. NPS support must not be assumed. An authenticated accept,
  reject, or accounting response demonstrates responsiveness in this mode.

All modes send Message-Authenticator and **require both a valid Response
Authenticator and a valid Message-Authenticator in the response**. This deliberate
strict policy can reject older servers, including implementations where
Message-Authenticator was optional. Fix/upgrade the server configuration instead
of weakening validation. Replies with an incorrect identifier or authentication
are discarded while waiting for a valid reply within the original deadline; if
none arrives, the last validation error is reported. Packets from other endpoints
are ignored by the connected UDP socket. An Access-Challenge never means that
authentication completed.

Register the probe's source IP as a RADIUS client with the same shared secret.
`nas-identifier` defaults to `nsclient-monitor` and can select the intended network
policy. No NAS-IP-Address or vendor-specific attributes are synthesized.

#### Credentials and status

Use `secret-file` and, for authentication, `password-file`. Files contain one
nonempty line; one trailing LF or CRLF is removed, while spaces are preserved.
Protect them so only the agent account and administrators can read them. Secrets
and passwords are not accepted directly on the command line and are never check
keywords. PAP password hiding is not transport encryption: use the probe on a
protected network and a limited test identity. Avoid identities that trigger
interactive MFA or production account lockouts.

The default critical expression is `result != 'ok'`. `reply` remains `none` until
the response passes authentication. Configuration errors, unreadable credential
files, and unavailable cryptographic support return UNKNOWN. Network and protocol
failures return CRITICAL by default. Add `warning=time > 500` for a latency alert.
The command does not create accounting sessions or support RadSec.

Protocol references: [RFC 2865](https://www.rfc-editor.org/rfc/rfc2865),
[RFC 3579](https://www.rfc-editor.org/rfc/rfc3579), and
[RFC 5997](https://www.rfc-editor.org/rfc/rfc5997).
