**Send a check result as email:**

```
submit_smtp target=mail command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

The defaults (`nscp@localhost` for both sender and recipient) exist to make the
module start, not to be used — public providers reject mail from an address they
do not consider yours.

```ini
[/settings/smtp/client/targets/mail]
address = smtp://smtp.example.com:587
security = starttls
username = alerts@example.com
password = <app password>
sender = alerts@example.com
recipient = ops@example.com
subject = [NSClient++] %source%
template = %source% reports: %message%
```

**Compose the mail from the command line:**

```
submit_smtp target=mail command=check_drivesize result=CRITICAL "message=/var is full" "subject=[ALERT] %source%" "template=%source%: %message%"
OK: Message submitted
```

`%source%` is the originating check name and `%message%` the plugin output. Put
the status in the subject if the mail is going to a phone.

**One recipient per submission:**

`recipient` is both `RCPT TO` and the `To:` header, and takes a single address.
Use a distribution list on the mail server to reach several people.

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 15m
channel = SMTP
```

Be sparing about which checks you attach: one submitted result is one email, and
there is no throttling here, so a flapping check sends a lot of mail.

**A password with `security = none` is refused, not sent in the clear:**

```
submit_smtp target=mail security=none
UNKNOWN: SMTP send failed: refusing to send AUTH credentials in clear; set security=starttls or security=tls
```

Fix the security setting rather than removing the password.

**Other failures the server reports:**

```
submit_smtp target=mail security=starttls
UNKNOWN: SMTP send failed: server did not advertise STARTTLS but security=starttls was requested

submit_smtp target=mail
UNKNOWN: SMTP send failed: a username is configured but the server does not advertise AUTH: 250-smtp.example.com
```

**Nothing listening:**

```
submit_smtp host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" sender=alerts@example.com recipient=ops@example.com
UNKNOWN: SMTP send failed: connect failed: Connection refused
```

**Transport security:**

`starttls` (the default, port 587) connects in clear and upgrades before
authenticating; `tls` (alias `ssl`, port 465) is TLS from the first byte;
`none` is unencrypted. `ca` selects the verification bundle, and
`ehlo-hostname` overrides the name announced in EHLO, which some servers check
against forward/reverse DNS before accepting mail.
