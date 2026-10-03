# NCPAServer

*Available on Linux only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

A server that serves the Nagios NCPA HTTP API, so Nagios Core and XI can poll NSClient++ with the stock check_ncpa.py plugin and the XI NCPA wizard.

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

Inside double quotes only `\"` is an escape.

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
monitoring server check it. Unless the certificate and its key actually load (and belong together), the listener
refuses to start rather than send the token in clear, unless `allow insecure = true` is set.

#### Concurrency

Up to `threads` requests (default 10) are answered at the same time, so a slow check - an external script close to its
timeout - only delays the polls behind it once every thread is busy.

The NCPA server and the WEB server are separate listeners with separate credentials and allow-lists, so either can run
without the other.


## Enable module

To enable this module and allow using the commands you need to add `NCPAServer = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NCPAServer = enabled
```


## Configuration

| Path / Section                                  | Description       |
|-------------------------------------------------|-------------------|
| [/settings/NCPA/server](#ncpa-server)           | NCPA server       |
| [/settings/NCPA/server/log](#log-configuration) | Log configuration |
| [/settings/default](#default-values)            | Default values    |


### NCPA server <a id="/settings/NCPA/server"></a>

Section for the NCPA (NCPAServer) protocol: the Nagios NCPA HTTP API, polled with check_ncpa.py and the Nagios XI NCPA wizard.

| Key                                                             | Default Value                       | Description                     |
|-----------------------------------------------------------------|-------------------------------------|---------------------------------|
| [allow arguments](#allow-arguments)                             | false                               | ALLOW ARGUMENTS                 |
| [allow insecure](#allow-insecure-cleartext-http)                | false                               | ALLOW INSECURE (CLEARTEXT HTTP) |
| [allowed ciphers](#allowed-ciphers)                             |                                     | ALLOWED CIPHERS                 |
| [allowed hosts](#allowed-hosts)                                 | 127.0.0.1                           | Allowed hosts                   |
| [auth rate limit block seconds](#auth-rate-limit-block-seconds) | 60                                  | AUTH RATE LIMIT (BLOCK SECONDS) |
| [auth rate limit max failures](#auth-rate-limit-failures)       | 10                                  | AUTH RATE LIMIT (FAILURES)      |
| [backup token](#backup-token)                                   |                                     | BACKUP TOKEN                    |
| [cache allowed hosts](#cache-list-of-allowed-hosts)             | true                                | Cache list of allowed hosts     |
| [certificate](#tls-certificate)                                 | ${certificate-path}/certificate.pem | TLS CERTIFICATE                 |
| [certificate key](#tls-private-key)                             |                                     | TLS PRIVATE KEY                 |
| [plugins](#exposed-plugins)                                     | any                                 | EXPOSED PLUGINS                 |
| [port](#port-number)                                            | 5693                                | PORT NUMBER                     |
| [threads](#worker-threads)                                      | 10                                  | WORKER THREADS                  |
| [tls version](#tls-version)                                     | 1.2+                                | TLS VERSION                     |
| [token](#token)                                                 |                                     | TOKEN                           |


```ini
# Section for the NCPA (NCPAServer) protocol: the Nagios NCPA HTTP API, polled with check_ncpa.py and the Nagios XI NCPA wizard.
[/settings/NCPA/server]
allow arguments=false
allow insecure=false
allowed hosts=127.0.0.1
auth rate limit block seconds=60
auth rate limit max failures=10
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
plugins=any
port=5693
threads=10
tls version=1.2+
```

#### ALLOW ARGUMENTS <a id="/settings/NCPA/server/allow arguments"></a>

Whether a request to the plugins node may carry arguments - path segments after the query name (check_ncpa -a) or \`args=\` parameters. False (the default) runs only the commands the agent defines, exactly as the NRPE server's \`allow arguments\`; define an alias to give a check fixed arguments.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | allow arguments                                 |
| Default value: | `false`                                         |


**Sample:**

```
[/settings/NCPA/server]
# ALLOW ARGUMENTS
allow arguments=false
```

#### ALLOW INSECURE (CLEARTEXT HTTP) <a id="/settings/NCPA/server/allow insecure"></a>

When false (the default) the listener refuses to start without a TLS certificate rather than serve the token in clear. Set to true only behind a TLS-terminating proxy or on loopback. Note that check_ncpa.py always connects with https.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | allow insecure                                  |
| Default value: | `false`                                         |


**Sample:**

```
[/settings/NCPA/server]
# ALLOW INSECURE (CLEARTEXT HTTP)
allow insecure=false
```

#### ALLOWED CIPHERS <a id="/settings/NCPA/server/allowed ciphers"></a>

OpenSSL cipher list the listener is restricted to. Empty (the default) leaves the library's own selection in place.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | allowed ciphers                                 |
| Default value: | _N/A_                                           |


**Sample:**

```
[/settings/NCPA/server]
# ALLOWED CIPHERS
allowed ciphers=
```

#### Allowed hosts <a id="/settings/NCPA/server/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | allowed hosts                                   |
| Default value: | `127.0.0.1`                                     |


**Sample:**

```
[/settings/NCPA/server]
# Allowed hosts
allowed hosts=127.0.0.1
```

#### AUTH RATE LIMIT (BLOCK SECONDS) <a id="/settings/NCPA/server/auth rate limit block seconds"></a>

How long a blocked address stays blocked. Default 60 s, doubling for an address that keeps guessing at machine speed, up to an hour - the same limiter the WEB server uses.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | auth rate limit block seconds                   |
| Default value: | `60`                                            |


**Sample:**

```
[/settings/NCPA/server]
# AUTH RATE LIMIT (BLOCK SECONDS)
auth rate limit block seconds=60
```

#### AUTH RATE LIMIT (FAILURES) <a id="/settings/NCPA/server/auth rate limit max failures"></a>

How many consecutive wrong tokens from one address block it. Default 10; 0 disables the limiter.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | auth rate limit max failures                    |
| Default value: | `10`                                            |


**Sample:**

```
[/settings/NCPA/server]
# AUTH RATE LIMIT (FAILURES)
auth rate limit max failures=10
```

#### BACKUP TOKEN <a id="/settings/NCPA/server/backup token"></a>

A second accepted token (NCPA's \`backup_community_string\`), so the token can be rotated without a window where pollers fail. Empty (the default) accepts only \`token\`.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | backup token                                    |
| Default value: | _N/A_                                           |


**Sample:**

```
[/settings/NCPA/server]
# BACKUP TOKEN
backup token=
```

#### Cache list of allowed hosts <a id="/settings/NCPA/server/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | cache allowed hosts                             |
| Default value: | `true`                                          |


**Sample:**

```
[/settings/NCPA/server]
# Cache list of allowed hosts
cache allowed hosts=true
```

#### TLS CERTIFICATE <a id="/settings/NCPA/server/certificate"></a>

The certificate the listener serves. The default is the same file the WEB server uses, so one certificate serves both; a default one is generated when it is missing. check_ncpa.py only verifies it when run with -s.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | certificate                                     |
| Default value: | `${certificate-path}/certificate.pem`           |


**Sample:**

```
[/settings/NCPA/server]
# TLS CERTIFICATE
certificate=${certificate-path}/certificate.pem
```

#### TLS PRIVATE KEY <a id="/settings/NCPA/server/certificate key"></a>

The private key for the certificate if it is not in the same file.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | certificate key                                 |
| Default value: | _N/A_                                           |


**Sample:**

```
[/settings/NCPA/server]
# TLS PRIVATE KEY
certificate key=
```

#### EXPOSED PLUGINS <a id="/settings/NCPA/server/plugins"></a>

Which queries the plugins node exposes: \`any\` (every registered query - checks, aliases and external scripts), \`scripts\` (only the commands CheckExternalScripts registers) or a comma-separated list of query names. A query that is not exposed answers exactly like one that does not exist.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | plugins                                         |
| Default value: | `any`                                           |


**Sample:**

```
[/settings/NCPA/server]
# EXPOSED PLUGINS
plugins=any
```

#### PORT NUMBER <a id="/settings/NCPA/server/port"></a>

Port to listen on. 5693 is the port check_ncpa.py and the XI wizard default to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | port                                            |
| Default value: | `5693`                                          |


**Sample:**

```
[/settings/NCPA/server]
# PORT NUMBER
port=5693
```

#### WORKER THREADS <a id="/settings/NCPA/server/threads"></a>

How many requests are answered at the same time. A check blocks the thread answering it until it returns, so a slow external script only delays the polls behind it once all threads are busy. Default 10.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | threads                                         |
| Default value: | `10`                                            |


**Sample:**

```
[/settings/NCPA/server]
# WORKER THREADS
threads=10
```

#### TLS VERSION <a id="/settings/NCPA/server/tls version"></a>

Which TLS versions the listener negotiates, in the same vocabulary as the WEB server: an exact version (1.0, 1.1, 1.2, 1.3), a trailing + for that version or later, or \`any\`. Honoured on builds using the beast web backend (all Linux packages).


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | tls version                                     |
| Default value: | `1.2+`                                          |


**Sample:**

```
[/settings/NCPA/server]
# TLS VERSION
tls version=1.2+
```

#### TOKEN <a id="/settings/NCPA/server/token"></a>

The NCPA token (NCPA's \`community_string\`), passed by check_ncpa.py as -t. There is no default: until one is set every request is refused. Compared in constant time, and never logged.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NCPA/server](#/settings/NCPA/server) |
| Key:           | token                                           |
| Default value: | _N/A_                                           |


**Sample:**

```
[/settings/NCPA/server]
# TOKEN
token=
```

### Log configuration <a id="/settings/NCPA/server/log"></a>

Configure which messages from the NCPA listener are logged.

| Key                  | Default Value | Description |
|----------------------|---------------|-------------|
| [debug](#log-debug)  | false         | LOG DEBUG   |
| [error](#log-errors) | true          | LOG ERRORS  |
| [info](#log-info)    | false         | LOG INFO    |


```ini
# Configure which messages from the NCPA listener are logged.
[/settings/NCPA/server/log]
debug=false
error=true
info=false
```

#### LOG DEBUG <a id="/settings/NCPA/server/log/debug"></a>

Log debug messages from the HTTP listener.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/NCPA/server/log](#/settings/NCPA/server/log) |
| Key:           | debug                                                   |
| Default value: | `false`                                                 |


**Sample:**

```
[/settings/NCPA/server/log]
# LOG DEBUG
debug=false
```

#### LOG ERRORS <a id="/settings/NCPA/server/log/error"></a>

Log errors from the HTTP listener.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/NCPA/server/log](#/settings/NCPA/server/log) |
| Key:           | error                                                   |
| Default value: | `true`                                                  |


**Sample:**

```
[/settings/NCPA/server/log]
# LOG ERRORS
error=true
```

#### LOG INFO <a id="/settings/NCPA/server/log/info"></a>

Log informational messages from the HTTP listener.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/NCPA/server/log](#/settings/NCPA/server/log) |
| Key:           | info                                                    |
| Default value: | `false`                                                 |


**Sample:**

```
[/settings/NCPA/server/log]
# LOG INFO
info=false
```

### Default values <a id="/settings/default"></a>

Default values used in other config sections.

| Key                                                 | Default Value | Description                 |
|-----------------------------------------------------|---------------|-----------------------------|
| [allowed hosts](#allowed-hosts)                     | 127.0.0.1     | Allowed hosts               |
| [bind to](#bind-to-address)                         |               | BIND TO ADDRESS             |
| [cache allowed hosts](#cache-list-of-allowed-hosts) | true          | Cache list of allowed hosts |
| [encoding](#nrpe-payload-encoding)                  |               | NRPE PAYLOAD ENCODING       |
| [inbox](#inbox)                                     | inbox         | INBOX                       |
| [password](#password)                               |               | Password                    |
| [socket queue size](#listen-queue)                  | 0             | LISTEN QUEUE                |
| [thread pool](#thread-pool)                         | 10            | THREAD POOL                 |
| [timeout](#timeout)                                 | 30            | TIMEOUT                     |
| [timezone](#timezone)                               | local         | Timezone                    |


```ini
# Default values used in other config sections.
[/settings/default]
allowed hosts=127.0.0.1
cache allowed hosts=true
inbox=inbox
socket queue size=0
thread pool=10
timeout=30
timezone=local
```

#### Allowed hosts <a id="/settings/default/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | allowed hosts                           |
| Default value: | `127.0.0.1`                             |


**Sample:**

```
[/settings/default]
# Allowed hosts
allowed hosts=127.0.0.1
```

#### BIND TO ADDRESS <a id="/settings/default/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | bind to                                 |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```

#### Cache list of allowed hosts <a id="/settings/default/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | cache allowed hosts                     |
| Default value: | `true`                                  |


**Sample:**

```
[/settings/default]
# Cache list of allowed hosts
cache allowed hosts=true
```

#### NRPE PAYLOAD ENCODING <a id="/settings/default/encoding"></a>




| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | encoding                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```

#### INBOX <a id="/settings/default/inbox"></a>

The default channel to post incoming messages on


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | inbox                                   |
| Default value: | `inbox`                                 |


**Sample:**

```
[/settings/default]
# INBOX
inbox=inbox
```

#### Password <a id="/settings/default/password"></a>

Password an inbound caller has to present. Stored hashed (pbkdf2-sha256$...) when written by \`nscp web install\` or \`nscp web password --set\`; a clear-text value written by hand is still accepted, and is hashed in place when re-set. This is a password to verify against, not key material: NSCA encrypts with its shared secret instead of verifying it, so it keeps its own key under /settings/NSCA/server (or the NSCAClient default target) and never reads this one.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | password                                |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# Password
password=
```

#### LISTEN QUEUE <a id="/settings/default/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | socket queue size                       |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `0`                                     |


**Sample:**

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```

#### THREAD POOL <a id="/settings/default/thread pool"></a>




| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | thread pool                             |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `10`                                    |


**Sample:**

```
[/settings/default]
# THREAD POOL
thread pool=10
```

#### TIMEOUT <a id="/settings/default/timeout"></a>

Timeout (in seconds) when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timeout                                 |
| Default value: | `30`                                    |


**Sample:**

```
[/settings/default]
# TIMEOUT
timeout=30
```

#### Timezone <a id="/settings/default/timezone"></a>

Timezone used to render dates such as boot time. Accepts 'local' (default), 'utc', or any POSIX TZ string parseable by Boost.Date_time (e.g. 'MST-07' or 'EST-05EDT,M3.2.0,M11.1.0').


| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timezone                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `local`                                 |


**Sample:**

```
[/settings/default]
# Timezone
timezone=local
```
