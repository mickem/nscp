# SMTPClient

SMTP client can be used both from command line and from queries to check remote systems via SMTP

## Enable module

To enable this module and allow using the commands you need to add `SMTPClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
SMTPClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the SMTPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_smtp](#submit_smtp) | Submit information to the remote SMTP server. |

### submit_smtp

Submit information to the remote SMTP server.

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

**Jump to section:**

* [Sample Commands](#submit_smtp_samples)
* [Command-line Arguments](#submit_smtp_options)


<a id="submit_smtp_samples"></a>
#### Sample Commands

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



<a id="submit_smtp_options"></a>
#### Command-line Arguments

<a id="submit_smtp_host"></a>
<a id="submit_smtp_port"></a>
<a id="submit_smtp_address"></a>
<a id="submit_smtp_timeout"></a>
<a id="submit_smtp_target"></a>
<a id="submit_smtp_retry"></a>
<a id="submit_smtp_retries"></a>
<a id="submit_smtp_source-host"></a>
<a id="submit_smtp_sender-host"></a>
<a id="submit_smtp_command"></a>
<a id="submit_smtp_alias"></a>
<a id="submit_smtp_message"></a>
<a id="submit_smtp_result"></a>
<a id="submit_smtp_separator"></a>
<a id="submit_smtp_batch"></a>
<a id="submit_smtp_sender"></a>
<a id="submit_smtp_recipient"></a>
<a id="submit_smtp_subject"></a>
<a id="submit_smtp_template"></a>
<a id="submit_smtp_username"></a>
<a id="submit_smtp_password"></a>
<a id="submit_smtp_security"></a>
<a id="submit_smtp_ca"></a>
<a id="submit_smtp_ehlo-hostname"></a>

| Option                                                    | Default Value | Description                                                                                        |
|-----------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------|
| host                                                      |               | The host of the host running the server                                                            |
| port                                                      |               | The port of the host running the server                                                            |
| address                                                   |               | The address (host:port) of the host running the server                                             |
| timeout                                                   |               | Number of seconds before connection times out (default=10)                                         |
| target                                                    |               | Target to use (lookup connection info from config)                                                 |
| retry                                                     |               | Number of times ti retry a failed connection attempt (default=2)                                   |
| retries                                                   |               | legacy version of retry                                                                            |
| source-host                                               |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| sender-host                                               |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| command                                                   |               | The name of the command that the remote daemon should run                                          |
| alias                                                     |               | Same as command                                                                                    |
| message                                                   |               | Message                                                                                            |
| result                                                    |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                             |
| separator                                                 |               | Separator to use for the batch command (default is |)                                              |
| batch                                                     |               | Add multiple records using the separator format is: command|result|message                         |
| sender                                                    |               | Envelope sender / From: header.                                                                    |
| recipient                                                 |               | Recipient address (one per submission).                                                            |
| subject                                                   |               | Subject template; %source% / %message% are substituted.                                            |
| template                                                  |               | Body template; %source% / %message% are substituted.                                               |
| username                                                  |               | SMTP AUTH username.                                                                                |
| password                                                  |               | SMTP AUTH password.                                                                                |
| security                                                  |               | Transport security: none | starttls (default) | tls (alias ssl).                                   |
| ca                                                        |               | CA bundle used to verify the server certificate (default: the agent's trusted bundle, ${ca-path}). |
| ehlo-hostname                                             |               | Hostname to send in EHLO.                                                                          |
| [insecure-skip-verify](#submit_smtp_insecure-skip-verify) | true          | Skip TLS certificate validation (test environments only).                                          |



<h5 id="submit_smtp_insecure-skip-verify">insecure-skip-verify:</h5>

Skip TLS certificate validation (test environments only).

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/SMTP/client](#smtp-client-section)               | SMTP CLIENT SECTION       |
| [/settings/SMTP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/SMTP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### SMTP CLIENT SECTION <a id="/settings/SMTP/client"></a>

Section for SMTP passive check module.

| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | SMTP          | CHANNEL     |


```ini
# Section for SMTP passive check module.
[/settings/SMTP/client]
channel=SMTP
```

#### CHANNEL <a id="/settings/SMTP/client/channel"></a>

The channel to listen to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/SMTP/client](#/settings/SMTP/client) |
| Key:           | channel                                         |
| Default value: | `SMTP`                                          |


**Sample:**

```
[/settings/SMTP/client]
# CHANNEL
channel=SMTP
```

### CLIENT HANDLER SECTION <a id="/settings/SMTP/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/SMTP/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description         |
|---------------------|---------------|---------------------|
| address             |               | TARGET ADDRESS      |
| allow host override | false         | ALLOW HOST OVERRIDE |
| host                |               | TARGET HOST         |
| port                |               | TARGET PORT         |
| retries             | 3             | RETRIES             |
| timeout             | 30            | TIMEOUT             |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/SMTP/client/targets/sample]
#address=...
allow host override=false
#host=...
#port=...
retries=3
timeout=30

```





