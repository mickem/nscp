`NCPAServer` serves the [Nagios Cross-Platform Agent](https://github.com/NagiosEnterprises/ncpa) (NCPA) HTTP API on
port `5693`, so Nagios Core and Nagios XI can poll NSClient++ with the stock `check_ncpa.py` plugin and the XI NCPA
wizard, unmodified. The module measures nothing itself: it authenticates the request with the NCPA token and answers
it from the agent's own checks.

This first release serves the `plugins/` node, which puts every NSClient++ check, alias and external script behind
`check_ncpa.py -M plugins/<name>`. The built-in NCPA node tree (`cpu/`, `memory/`, `disk/`, `interface/`,
`services/`, `processes/`, `system/`, `user/`) is not served yet; asking for one of those nodes is answered the way
NCPA answers any node that does not exist.

To use this module you need to enable it and give it a token:

```
[/modules]
NCPAServer = enabled

[/settings/NCPA/server]
token = <a long random string>

[/settings/default]
allowed hosts = 127.0.0.1,192.168.0.10
```

#### Running checks with check_ncpa.py

The `-a` string is split into tokens, and each token is passed to the check as one argument, the way the REST API
passes `key=value`. Quote a token that contains spaces:

```
check_ncpa.py -H agent.example.com -t '<token>' -M plugins/check_cpu
check_ncpa.py -H agent.example.com -t '<token>' -M plugins/check_cpu -a '"warning=load > 80" "critical=load > 90"'
check_ncpa.py -H agent.example.com -t '<token>' -M plugins -l
```

The answer is the check's own output, unchanged: the Nagios exit code becomes `returncode` and the message, followed by
`|` and the performance data, becomes `stdout`. The `-w`, `-c`, `-u` and `-n` options of `check_ncpa.py` do not
apply to `plugins/`: the thresholds belong in the check's own arguments.

`check_ncpa.py -M plugins -l` lists the queries the server exposes, aliases included.

The arguments are split the way NCPA splits them: any `args=` parameters first, then the path segments after the query
name, joined with spaces and split again. The quoting rules are those of the NSClient++ prompt:

| Written                       | Reaches the check as        | Why                                                         |
|-------------------------------|-----------------------------|-------------------------------------------------------------|
| `"warning=load > 80"`         | `warning=load > 80`         | double quotes group and are removed                         |
| `path='C:\Program Files\app'` | `path=C:\Program Files\app` | a single quote groups where a whole value starts            |
| `filter=core='total'`         | `filter=core='total'`       | a single quote anywhere else is kept, for the filter syntax |
| `path=C:\Windows\Temp`        | `path=C:\Windows\Temp`      | a backslash is an ordinary character                        |

A backslash is never an escape, not even before a quote, so `"path=C:\Temp\"` ends where it appears to. To pass a
double quote, put the value in single quotes.

#### Securing the server

*   **`token`** - required. There is no default: until one is set every request is refused, and the log says why. The
    plugin passes it with `-t`, and it travels in the query string, so it is only as private as the connection: keep
    TLS on. The token is compared in constant time and never logged. A wrong or missing token is answered with
    `{"error": "Incorrect credentials given."}`, which `check_ncpa.py` reports as `CRITICAL`, and ten wrong tokens in a
    row from one address block it for a minute (`auth rate limit max failures`, `auth rate limit block seconds`). A
    block survives a settings reload.
*   **`backup token`** - a second accepted token, so the token can be rotated without a window where pollers fail.
*   **`allowed hosts`** - which addresses may connect at all, inherited from `[/settings/default]`. A connection from
    a host outside the list is closed as soon as it is accepted, before the TLS handshake, and logged - once a minute
    per address, with a count of the refusals left out in between. Host names in the list are resolved when the
    listener starts and, with `cache allowed hosts = false`, again for every connection, as on the other servers.
*   **`allow arguments`** - `false` by default, with the same meaning as the NRPE server's: a request to `plugins/`
    that carries arguments is refused with `UNKNOWN`. The caller can run the commands the agent defines but cannot
    shape what they do. Define an alias to give a check fixed arguments, or set `allow arguments = true`.
*   **`plugins`** - which queries `plugins/` exposes: `any` (the default, every registered query), `scripts` (only
    what CheckExternalScripts registers) or a comma-separated list of names. A query that is not exposed is answered
    exactly like one that does not exist.

#### TLS

The listener serves HTTPS with the same certificate as the WEB server (`${certificate-path}/certificate.pem`, generated
on first start when missing), so one certificate serves both. `check_ncpa.py` does not verify the certificate unless it
is run with `-s`, so the generated self-signed one works out of the box; give it a real one and pass `-s` to have the
monitoring server check it. Unless the certificate and its key actually load (and belong together), the listener
refuses to start rather than send the token in clear.

To serve plain HTTP, for instance behind a TLS-terminating proxy, both settings are needed: an empty `certificate`
(otherwise the default one is generated and used) and `allow insecure = true` (otherwise a missing certificate stops
the listener):

```
[/settings/NCPA/server]
certificate =
allow insecure = true
```

#### Listening address

The listener binds to `bind to`, set under `[/settings/NCPA/server]` or inherited from `[/settings/default]` like the
NRPE and NSCA servers, and to every interface when that is empty. An IPv6 address is written bare (`bind to = ::1`).

#### Concurrency

Up to `threads` requests (default 10) are answered at the same time, so a slow check - an external script close to its
timeout - only delays the polls behind it once every thread is busy.

The NCPA server and the WEB server are separate listeners with separate credentials and allow-lists, so either can run
without the other.
