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

The `-a` string is split like a shell command line, and each resulting token is passed to the check as one argument,
the way the REST API passes `key=value`. Quote a token that contains spaces:

```
check_ncpa.py -H agent.example.com -t '<token>' -M plugins/check_cpu
check_ncpa.py -H agent.example.com -t '<token>' -M plugins/check_cpu -a '"warning=load > 80" "critical=load > 90"'
check_ncpa.py -H agent.example.com -t '<token>' -M plugins -l
```

The answer is the check's own output, unchanged: the Nagios exit code becomes `returncode` and the message, followed by
`|` and the performance data, becomes `stdout`. The `-w`, `-c`, `-u` and `-n` options of `check_ncpa.py` do not
apply to `plugins/`: the thresholds belong in the check's own arguments.

`check_ncpa.py -M plugins -l` lists the queries the server exposes.

#### Securing the server

*   **`token`** - required. There is no default: until one is set every request is refused, and the log says why. The
    plugin passes it with `-t`, and it travels in the query string, so it is only as private as the connection: keep
    TLS on. The token is compared in constant time and never logged. A wrong or missing token is answered with
    `{"error": "Incorrect credentials given."}`, which `check_ncpa.py` reports as `CRITICAL`, and ten wrong tokens in a
    row from one address block it for a minute (`auth rate limit max failures`, `auth rate limit block seconds`).
*   **`backup token`** - a second accepted token, so the token can be rotated without a window where pollers fail.
*   **`allowed hosts`** - which addresses may connect at all, inherited from `[/settings/default]`. A host outside the
    list gets a plain HTTP 403.
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
monitoring server check it. Without a certificate the listener refuses to start rather than send the token in clear,
unless `allow insecure = true` is set.

The NCPA server and the WEB server are separate listeners with separate credentials and allow-lists, so either can run
without the other.
