# NSCAClient

NSCA client can be used both from command line and from queries to submit passive checks via NSCA

## Enable module

To enable this module and allow using the commands you need to add `NSCAClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NSCAClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the NSCAClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nsca](#submit_nsca) | Submit information to the remote NSCA server. |

### submit_nsca

Submit information to the remote NSCA server.

#### About `submit_nsca`

`submit_nsca` submits a passive check result to an **NSCA** daemon — the classic
Nagios passive-result transport.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nsca`, or add `NSCA` to the channels a check reports on,
and each result is submitted as it is produced. `NSCA` is the default channel
for [`check_and_forward`](../check/CheckHelpers.md#check_and_forward). A direct
call is mainly useful for verifying that the password, cipher and payload length
match the receiving daemon.

##### Three settings must match the server exactly

NSCA has no negotiation: the client and the daemon must agree up front, and a
mismatch shows up as a silently dropped or garbled result rather than an error.

- **`encryption`** — the cipher, defaulting to `aes`. It must equal the
  `decryption_method` in the daemon's `nsca.cfg`.
- **`password`** — the shared secret, matching the daemon's `password`.
- **`payload length`** — 512 by default, matching NSCA's compiled-in
  `MAX_PACKETSIZE`. A daemon built with a larger payload needs the same value
  here, and a mismatch truncates or corrupts every message.

If results simply never appear on the server, check these three before anything
else.

##### What NSCA's "encryption" is and is not

The NSCA ciphers are a shared-secret obfuscation over a plain TCP connection.
There is no server authentication, no replay protection and no integrity
guarantee, and `encryption = none` disables even the obfuscation. It should not
be relied on across an untrusted network.

This module can additionally wrap the connection in **TLS** — `certificate`,
`certificate key`, `ca`, `dh` and `allowed ciphers` — which is what actually
authenticates the peer. Where the receiving side supports it, prefer that, or
consider [NSCA-ng](NSCANgClient.md) instead, which was designed with TLS from
the start.

`encoding` sets the character encoding used for the message text, which matters
when check output contains non-ASCII characters.

**Jump to section:**

* [Sample Commands](#submit_nsca_samples)
* [Command-line Arguments](#submit_nsca_options)


<a id="submit_nsca_samples"></a>
#### Sample Commands

**Submit a passive result to an NSCA daemon:**

```
submit_nsca host=192.168.56.10 port=5667 command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_nsca host=192.168.56.10 "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**The usual arrangement — route results rather than calling this by hand:**

`NSCA` is the default channel for
[`check_and_forward`](../check/CheckHelpers.md#check_and_forward):

```ini
[/settings/NSCA/client/targets/default]
address = nsca://192.168.56.10:5667
encryption = aes
password = <shared secret>
payload length = 512
```

```
check_and_forward command=check_drivesize channel=NSCA alias=drivesize
OK: Message submitted: NSCA
```

**The three settings that must match the daemon exactly:**

`encryption` must equal the daemon's `decryption_method`, `password` its
`password`, and `payload length` its compiled-in `MAX_PACKETSIZE` (512 by
default). NSCA negotiates nothing, so a mismatch shows up as a dropped or
garbled result rather than an error — check these three first when results never
appear on the server.

**When the requested cipher is not in this build:**

Crypto++ is an optional dependency, so a package built without it offers only a
reduced set. The error names what is actually available:

```
submit_nsca host=192.168.56.10 command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: NSCA error: Unknown encryption algorithm: 'aes' (available: none = No Encryption (not safe), xor = XOR; use 'none' to disable encryption)
```

**When the far end is not an NSCA daemon:**

NSCA expects the server to open the exchange with an init packet, so a plain
listener on the port fails the handshake rather than accepting the result:

```
submit_nsca host=127.0.0.1 port=5667 command=nightly_backup result=CRITICAL "message=backup failed" encryption=none
UNKNOWN: Error: Retry failed
```

**A note on "encryption":**

The NSCA ciphers are shared-secret obfuscation over plain TCP — no server
authentication, no replay protection, no integrity. Wrap the connection in TLS
(`certificate`, `certificate key`, `ca`, `dh`, `allowed ciphers`) where the
receiving side supports it, or use [NSCA-ng](NSCANgClient.md) instead.



<a id="submit_nsca_options"></a>
#### Command-line Arguments

<a id="submit_nsca_host"></a>
<a id="submit_nsca_port"></a>
<a id="submit_nsca_address"></a>
<a id="submit_nsca_timeout"></a>
<a id="submit_nsca_target"></a>
<a id="submit_nsca_retry"></a>
<a id="submit_nsca_retries"></a>
<a id="submit_nsca_source-host"></a>
<a id="submit_nsca_sender-host"></a>
<a id="submit_nsca_command"></a>
<a id="submit_nsca_alias"></a>
<a id="submit_nsca_message"></a>
<a id="submit_nsca_result"></a>
<a id="submit_nsca_separator"></a>
<a id="submit_nsca_batch"></a>
<a id="submit_nsca_certificate"></a>
<a id="submit_nsca_dh"></a>
<a id="submit_nsca_certificate-key"></a>
<a id="submit_nsca_certificate-format"></a>
<a id="submit_nsca_ca"></a>
<a id="submit_nsca_verify"></a>
<a id="submit_nsca_allowed-ciphers"></a>
<a id="submit_nsca_payload-length"></a>
<a id="submit_nsca_buffer-length"></a>
<a id="submit_nsca_password"></a>
<a id="submit_nsca_hostname"></a>
<a id="submit_nsca_time-offset"></a>
<a id="submit_nsca_timezone"></a>

| Option                                | Default Value | Description                                                                                                                                                               |
|---------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                                  |               | The host of the host running the server                                                                                                                                   |
| port                                  |               | The port of the host running the server                                                                                                                                   |
| address                               |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                               |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                                |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                                 |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                               |               | legacy version of retry                                                                                                                                                   |
| source-host                           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                               |               | The name of the command that the remote daemon should run                                                                                                                 |
| alias                                 |               | Same as command                                                                                                                                                           |
| message                               |               | Message                                                                                                                                                                   |
| result                                |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| separator                             |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                                 |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| certificate                           |               | The client certificate to use                                                                                                                                             |
| dh                                    |               | The DH key to use                                                                                                                                                         |
| certificate-key                       |               | Client certificate to use                                                                                                                                                 |
| certificate-format                    |               | Client certificate format                                                                                                                                                 |
| ca                                    |               | Certificate authority                                                                                                                                                     |
| verify                                |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                       |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nsca_ssl)               | true          | Initial an ssl handshake with the server.                                                                                                                                 |
| [encryption](#submit_nsca_encryption) |               | Name of encryption algorithm to use.                                                                                                                                      |
| payload-length                        |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length                         |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |
| password                              |               | Password                                                                                                                                                                  |
| hostname                              |               | Host name to report                                                                                                                                                       |
| time-offset                           |               |                                                                                                                                                                           |
| timezone                              |               | Reference timezone for wire timestamps (default 'utc'; use 'local' only for legacy peers that emit local-clock-as-Unix-time)                                              |



<h5 id="submit_nsca_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`

<h5 id="submit_nsca_encryption">encryption:</h5>

Name of encryption algorithm to use.
Has to be the same as your server i using or it wont work at all.This is also independent of SSL and generally used instead of SSL.
Available encryption algorithms are:
none = No Encryption (not safe)
xor = XOR
des = DES
3des = DES-EDE3
cast128 = CAST-128
xtea = XTEA
blowfish = Blowfish
twofish = Twofish
rc2 = RC2
aes128 = AES
aes192 = AES
aes = AES
serpent = Serpent
gost = GOST



This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NSCA/client](#nsca-client-section)               | NSCA CLIENT SECTION       |
| [/settings/NSCA/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCA/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### NSCA CLIENT SECTION <a id="/settings/NSCA/client"></a>

Section for NSCA passive check module.

| Key                             | Default Value | Description        |
|---------------------------------|---------------|--------------------|
| [channel](#channel)             | NSCA          | CHANNEL            |
| [encoding](#nsca-data-encoding) |               | NSCA DATA ENCODING |
| [hostname](#hostname)           | auto          | HOSTNAME           |


```ini
# Section for NSCA passive check module.
[/settings/NSCA/client]
channel=NSCA
hostname=auto
```

#### CHANNEL <a id="/settings/NSCA/client/channel"></a>

The channel to listen to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | channel                                         |
| Default value: | `NSCA`                                          |


**Sample:**

```
[/settings/NSCA/client]
# CHANNEL
channel=NSCA
```

#### NSCA DATA ENCODING <a id="/settings/NSCA/client/encoding"></a>




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | encoding                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |


**Sample:**

```
[/settings/NSCA/client]
# NSCA DATA ENCODING
encoding=
```

#### HOSTNAME <a id="/settings/NSCA/client/hostname"></a>

The host name of the monitored computer.
Set this to auto (default) to use the windows name of the computer.

auto	Hostname
${host}	Hostname
${host_lc}	Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase
${address_ipv4}	IPv4 address of the computer
${address_ipv6}	IPv6 address of the computer (lowercase, compressed)
${address_ipv6_lc}	IPv6 address in lowercase (compressed)
${address_ipv6_uc}	IPv6 address in uppercase (compressed)
${address_ipv6_lc_comp}	IPv6 address in lowercase, compressed (2001:db8::7)
${address_ipv6_lc_uncomp}	IPv6 address in lowercase, uncompressed (2001:0db8:0000:0000:0000:0000:0000:0007)
${address_ipv6_uc_comp}	IPv6 address in uppercase, compressed
${address_ipv6_uc_uncomp}	IPv6 address in uppercase, uncompressed



| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | hostname                                        |
| Default value: | `auto`                                          |


**Sample:**

```
[/settings/NSCA/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/NSCA/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCA/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description           |
|---------------------|---------------|-----------------------|
| address             |               | TARGET ADDRESS        |
| allow host override | false         | ALLOW HOST OVERRIDE   |
| allowed ciphers     |               | ALLOWED CIPHERS       |
| ca                  |               | CA                    |
| certificate         |               | SSL CERTIFICATE       |
| certificate format  |               | CERTIFICATE FORMAT    |
| certificate key     |               | SSL CERTIFICATE       |
| dh                  |               | DH KEY                |
| encoding            |               | ENCODING              |
| encryption          | aes           | ENCRYPTION            |
| host                |               | TARGET HOST           |
| password            |               | PASSWORD              |
| payload length      | 512           | PAYLOAD LENGTH        |
| port                |               | TARGET PORT           |
| retries             | 3             | RETRIES               |
| time offset         | 0             | TIME OFFSET           |
| timeout             | 30            | TIMEOUT               |
| timezone            | utc           | TIMEZONE              |
| use ssl             |               | ENABLE SSL ENCRYPTION |
| verify mode         |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCA/client/targets/sample]
#address=...
allow host override=false
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#encoding=...
encryption=aes
#host=...
#password=...
payload length=512
#port=...
retries=3
time offset=0
timeout=30
timezone=utc
#use ssl=...
#verify mode=...

```





