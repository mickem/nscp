#### About `submit_smtp`

`submit_smtp` sends a check result as an **email**. It is the "just tell someone"
destination for a host that has no monitoring server to report to, or as a
last-resort notification path alongside a real one.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=smtp`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying that the server
accepts the message.

##### Composing the mail

`sender` is both the envelope sender and the `From:` header, and `recipient`
both `RCPT TO` and `To:` — **one recipient per submission**; a distribution list
on the mail server is the way to reach several people.

`subject` (default `[NSClient++] %source%`) and `template` (the body) are
templates in which `%source%` is replaced by the originating check name and
`%message%` by the plugin output. Put the status and the message in the subject
if the mail is going to a phone.

The defaults (`nscp@localhost` for both sender and recipient) exist to make the
module start, not to be used. Public providers such as Gmail and Microsoft 365
reject mail from an address they do not consider yours, so set a real sender
before expecting delivery.

##### Transport security

`security` decides how the connection is protected:

- **`starttls`** (default, port 587) — connect in clear, then upgrade to TLS
  before authenticating.
- **`tls`** (alias `ssl`, port 465) — TLS from the first byte.
- **`none`** — no encryption at all.

`username` / `password` supply AUTH credentials. With `security = none` **and** a
password configured the submission **fails rather than sending the credentials in
clear** — a deliberate refusal, not a bug; fix the security setting rather than
removing the password.

`ca` selects the bundle used to verify the server. `insecure-skip-verify`
disables that verification and should be reserved for bringing up a server with
a self-signed certificate — with it on, `starttls` gives no protection against an
active attacker. `ehlo-hostname` overrides the name announced in EHLO, which
some servers check against forward/reverse DNS before accepting mail.

##### Rate

There is no throttling here: one submitted result is one email. Route a
flapping check to this target and you will send a lot of mail, so prefer
attaching it to a small number of deliberately chosen checks.
