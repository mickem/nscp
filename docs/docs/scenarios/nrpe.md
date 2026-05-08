# Active Monitoring with NRPE

**Goal:** Configure NSClient++ as an NRPE agent so a Nagios-style monitoring server can poll it for check results, with TLS encryption and (optionally) client-certificate authentication.

!!! tip
    NRPE is the right choice when the monitoring server can reach the agent
    directly. If the agent sits behind a firewall or NAT, look at
    [Passive Monitoring (NSCA/NRDP)](passive-monitoring-nsca.md) or
    [Passive Monitoring (Icinga 2)](passive-monitoring-icinga.md) instead.

---

## Prerequisites

Enable the `NRPEServer` module in `nsclient.ini`:

```ini
[/modules]
NRPEServer = enabled
```

Or activate from the command line:

```
nscp settings --activate-module NRPEServer
```

For the certificate-based examples below you'll also need
[mkcert](https://github.com/FiloSottile/mkcert) to generate a local CA and
some certs (or any other CA you control).

---

## Insecure version

When NRPE was first released the world was a different place and security was not a big concern.
So the first version of NRPE did not have any real authentication or encryption.

> You can still use this but it is not recommended.

To use the insecure version of NRPE you need to enable the insecure mode in NSClient++.
You can do this using the command line:

```commandline
$ nscp nrpe install --allowed-hosts 127.0.0.1 --insecure --arguments=safe
WARNING: Inconsistent insecure option will overwrite verify=peer-cert with none due to --insecure
Enabling NRPE via SSL from: 127.0.0.1 on port 5666
WARNING: NRPE is currently insecure.
SAFE Arguments are allowed.
```

Here we for convenience also allow `safe` arguments. `safe` arguments are arguments which do not contain `|&><'\"\\[]{}` to prevent shell execution.
This is not really useful as `NRPE Server` does not allow remote shell execution but as we do not know how `CheckExternalScripts` are configured it adds some weak security.

As always lets start NSClient++ in debug mode again:

```
nscp test
...
exit
```

And next lets run a check using the `check_nrpe` command line tool.

```commandline
$ check_nrpe -H 127.0.0.1 -2
I (0.4.0 2025-08-30) seem to be doing fine...
```

Here we set `-2` to use the old version of NRPE and `-d 1` to enable insecure ADH key exchange.

## Using certificates (still insecure)

Next up lets make this a bit more secure by using TLS certificates.

```commandline
$ nscp nrpe install --allowed-hosts 127.0.0.1 --insecure=false
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
WARNING: NRPE is currently insecure.
Arguments are NOT allowed.
```

As you can see it used the default certificate and not our custom certificate.
SO lets change that:

```commandline
$ nscp nrpe install ^
        --allowed-hosts 127.0.0.1 ^
        --insecure=false --verify=none ^
        --certificate ${certificate-path}\server.pem ^
        --certificate-key ${certificate-path}\server.key
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
WARNING: NRPE is not secure, while we have proper encryption there is no authentication expect for only accepting traffic from 127.0.0.1.
Traffic is encrypted using nrpe_test\server.crt and nrpe_test\server.key.
Arguments are NOT allowed.
```

Now we can restart NSClient++ in debug mode again:

```
nscp test
...
exit
```

And run the check again using the `check_nrpe` command line tool.

```commandline
$ check_nrpe -H 127.0.0.1 --ssl-version TLSv1.2+
I (0.4.0 2025-08-30) seem to be doing fine...
```

## Using client certificates

As you can see there is still no authentication but at least the traffic is encrypted.
To make this a bit better we can use client certificates to authenticate the client.

First we need to enable client certificate authentication in NSClient++.

```commandline
$ nscp nrpe install ^
        --allowed-hosts 127.0.0.1 ^
        --insecure=false --verify=peer-cert ^
        --certificate ${certificate-path}\server.pem ^
        --certificate-key ${certificate-path}\server.key ^
        --ca ${certificate-path}\ca.pem
Enabling NRPE via SSL from: 127.0.0.1 on port 5666.
NRPE is currently reasonably secure and will require client certificates.
The clients need to have a certificate issued from nrpe_test\ca.crt
Traffic is encrypted using nrpe_test\server.crt and nrpe_test\server.key.
Arguments are NOT allowed.
```

Then we also need to get the root certificate from mkcert.

```
mkcert -CAROOT
```

In this path you will find the `rootCA.pem` file which we need to copy to the NSClient++ security folder as `ca.pem`.

```
copy rootCA.pem "c:\program files\nsclient++\security\ca.pem"
```

Now we can restart NSClient++ in debug mode again:

```
nscp test
...
exit
```

Then we need to generate a client certificate using `mkcert`.

```
mkcert -client nagios
```

Next copy the generated files to the Nagios server.
Then we need to configure the `check_nrpe` command to use the client certificate.

```commandline
$ check_nrpe -H 127.0.0.1 -3 --cert nagios-client.pem --key nagios-client-key.pem
I (0.4.0 2025-08-30) seem to be doing fine...
```

Next to verify that the client certificate is required we can try to run the command without the certificate.

```commandline
$ check_nrpe -H 127.0.0.1 -3
CHECK_NRPE: Receive header underflow - only 0 bytes received (4 expected).
```

And that is it we now have a reasonably secure NRPE setup.

If you run into any issues I can recommend validating the connection using `openssl s_client`:

```commandline
openssl s_client -connect 127.0.0.1:5666 -cert nagios-client.pem -key nagios-client-key.pem
```

In general certificates can be a bit tricky to get right.

---

## Running Checks with Arguments

Once the connection works, the next step is running real checks. The
`check_nrpe` syntax is:

```
check_nrpe -H <agent> -c <command> [-a <arg> <arg> ...]
```

- `-c` selects the command to run on the agent (a built-in like `check_cpu`,
  or an alias defined in `nsclient.ini`).
- `-a` passes arguments **to** the command. Anything after `-a` is treated as
  a (space-separated) argument list.

Example — query CPU load with thresholds:

```commandline
$ check_nrpe -H 127.0.0.1 -c check_cpu -a "warn=load>80" "crit=load>90"
OK: CPU load is ok.|'total 5m'=0%;80;90 'total 1m'=1%;80;90 'total 5s'=2%;80;90
```

For this to work the agent has to **accept** arguments — see the next
section.

---

## Security Tunables: `allow arguments` and `allow nasty characters`

By default the NRPE server rejects all command-line arguments from the
network. Letting a remote machine pass arguments to a check has obvious
security implications, so NSClient++ exposes two flags:

| Setting                  | Default | Effect                                                                                                              |
|--------------------------|---------|---------------------------------------------------------------------------------------------------------------------|
| `allow arguments`        | `false` | Accept arguments via `check_nrpe -a ...`. Required if you want to drive thresholds from the monitoring server.      |
| `allow nasty characters` | `false` | Permit characters that can introduce shell-redirection (`<`, `>`, `\|`, `&`, etc.) in arguments. Rarely safe to enable. |

There are **two** independent copies of `allow arguments`: one on the NRPE
server and one on `CheckExternalScripts`. The NRPE one gates remote calls
into built-in commands; the external-scripts one gates remote-controlled
arguments to your own scripts. The latter is significantly more dangerous
(scripts can do anything an OS process can), so most operators only enable
the NRPE-side flag.

```ini
[/settings/NRPE/server]
allow arguments        = true
allow nasty characters = false       ; keep this off
allowed hosts          = 192.168.0.0/24
port                   = 5666
```

!!! danger
    Combine `allow arguments = true` with a tight `allowed hosts` list (and
    a firewall) so only your monitoring server can reach the NRPE port. Any
    host that can hit the port can pass arbitrary arguments to whatever
    commands you've defined.

!!! note "What `allow nasty characters` does today"
    Argument substitution into external-script templates is now isolated at
    the argv level: each `$ARGn$` reaches the child as a single argv
    element regardless of what shell metacharacters it contains. The launch
    path no longer goes through `/bin/sh -c` on Unix or relies on
    `CreateProcess` re-tokenising the command line on Windows.

    `allow nasty characters` is therefore defence in depth rather than the
    last line of defence. Leaving it `false` (the default) still blocks
    `|`, `` ` ``, `&`, `>`, `<`, `'`, `"`, `\`, `[`, `]`, `{`, `}` at the
    NRPE ingress, which catches the most obvious abuse patterns and trips
    misconfigured monitoring early.

---

## NRPE Protocol Versions

NSClient++ supports NRPE versions **2, 3, and 4** out of the box — there is
no configuration knob to pick between them. The `check_nrpe` client picks
the protocol via `-2` / `-3` / `-4`:

```
check_nrpe -H 127.0.0.1 -2 ...   # legacy v2
check_nrpe -H 127.0.0.1 -3 ...   # v3 (variable-length payload)
```

Older `check_nrpe` builds (anything pre-3.x) may not negotiate TLS 1.2 and
will fail against modern NSClient++. If you can't upgrade `check_nrpe`,
either upgrade the client or relax `tls version` on the agent — but only as
a temporary measure.

---

## Nagios Server-Side Configuration

The Nagios side is a quick reference rather than a full guide — your
distribution's Nagios docs go into more depth. Sketch:

**Template** for Windows hosts:

```cfg
define host {
    name                  tpl-windows
    use                   generic-host
    check_command         check-host-alive
    check_interval        5
    max_check_attempts    10
    register              0    ; template, don't register
}
```

**Host** definition referencing the template:

```cfg
define host {
    use        tpl-windows
    host_name  windowshost
    alias      My Windows Server
    address    10.0.0.2
}
```

**Service** definitions invoking `check_nrpe`:

```cfg
define service {
    use                  generic-service
    host_name            windowshost
    service_description  CPU Load
    check_command        check_nrpe!check_cpu
}

define service {
    use                  generic-service
    host_name            windowshost
    service_description  Free Space
    check_command        check_nrpe!check_drivesize
}
```

The token after `!` is what gets passed as the NRPE command name (i.e. the
`-c` value). For arguments, configure the `check_nrpe` command in your
`commands.cfg` to include `-a $ARG1$` and pass them via
`check_command check_nrpe!check_cpu!warn=load>80`.

---

## Troubleshooting

When something doesn't work, the fastest path to a diagnosis is **test
mode** — start NSClient++ in the foreground so you can see what it logs as
each request arrives:

```
net stop nscp
nscp test
```

You'll see lines like:

```
d NSClient++.cpp Loading plugin: NRPE server (w/ SSL)...
d NRPEServer.cpp Starting NRPE socket...
d Socket.h       Bound to: 0.0.0.0:5666
```

Now run a `check_nrpe` from the monitoring server and watch the log. A
request for an unknown command shows up as:

```
NSClient++.cpp Injecting: foobar:
NSClient++.cpp No handler for command: 'foobar'
```

Other useful tools:

- **`openssl s_client -connect 127.0.0.1:5666 -cert client.pem -key client.key`**
  to verify the TLS handshake independently of `check_nrpe`.
- **`nscp test`** is also where you can inject commands directly without going
  through the network — handy for telling a configuration problem apart from a
  network problem.

---

## Next Steps

- [Reference: NRPEServer](../reference/client/NRPEServer.md) — every NRPE server setting in detail
- [Passive Monitoring (NSCA/NRDP)](passive-monitoring-nsca.md) — the inverse pattern, when the agent can't be reached directly
- [Web Interface](../setup/web-interface.md) — manage the agent and its certificates from the browser
