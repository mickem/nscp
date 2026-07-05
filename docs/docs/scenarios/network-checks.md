# Network Checks

**Goal:** Monitor network connectivity and service availability — host reachability (ping), TCP port connectivity, and HTTP endpoint health.

---

## Prerequisites

Enable the `CheckNet` module in `nsclient.ini`:

```ini
[/modules]
CheckNet   = enabled
NRPEServer = enabled   ; if using NRPE
```

Or load it temporarily in the test shell:

```
load CheckNet
```

---

## Ping (ICMP Reachability)

Use `check_ping` to verify that a host responds to ICMP echo requests.

### Command

```
check_ping host=192.168.0.1
```

### Expected output (healthy)

```
OK: All 1 hosts are ok
'192.168.0.1 loss'=0%;5;10 '192.168.0.1 time'=2ms;60;100
```

### Expected output (alert)

```
CRITICAL: CRITICAL: 192.168.0.1: loss 100%
'192.168.0.1 loss'=100%;5;10 '192.168.0.1 time'=0ms;60;100
```

Default thresholds:
- **Warning:** response time > 60 ms or packet loss > 5%
- **Critical:** response time > 100 ms or packet loss > 10%

### Common scenarios

**Ping multiple hosts at once:**

```
check_ping host=192.168.0.1 host=8.8.8.8 host=google.com total
```

**Tighter thresholds:**

```
check_ping host=8.8.8.8 count=4 timeout=300 "warn=time > 30 or loss > 0%" "crit=time > 80 or loss > 25%"
```

**Custom output showing each host's latency:**

```
check_ping host=192.168.0.1 host=192.168.0.2 "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(time)ms"
```

```
OK: 192.168.0.1=2ms, 192.168.0.2=22ms
```

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_ping --argument "host=192.168.0.1"
```

---

## TCP Port Connectivity

Use `check_tcp` to verify that a TCP port is open and accepting connections.

### Command

```
check_tcp host=192.168.0.1 port=443
```

### Expected output (healthy)

```
OK: Connected to 192.168.0.1:443
'time'=5ms;100;500
```

### Expected output (alert — port closed or refused)

```
CRITICAL: CRITICAL: Failed to connect to 192.168.0.1:443
```

### Common scenarios

**Check multiple services:**

```
check_tcp host=myserver.example.com port=80   ; HTTP
check_tcp host=myserver.example.com port=443  ; HTTPS
check_tcp host=myserver.example.com port=3306 ; MySQL
check_tcp host=myserver.example.com port=1433 ; SQL Server
```

**Set a tighter connection timeout:**

```
check_tcp host=myserver.example.com port=443 timeout=2000 "warn=time > 200" "crit=time > 1000"
```

**Use a service preset** (sets the port and validates the greeting) — `ftp`, `pop`, `imap`, `smtp`, `ssh`, and the implicit-TLS variants `spop`, `simap`, `ssmtp`:

```
check_tcp host=mail.example.com service=smtp
check_tcp host=mail.example.com service=ssmtp   ; SMTP-over-TLS on port 465
```

**Wrap the connection in TLS** and (optionally) verify the server certificate or
pin a minimum protocol version (`tls-version` accepts `tlsv1.0`…`tlsv1.3`, `sslv3`):

```
check_tcp host=secure.example.com port=443 ssl=true
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt
check_tcp host=secure.example.com port=443 ssl=true tls-version=tlsv1.2+
```

**Match the peer's banner/response** with a regex via the `response` keyword:

```
check_tcp host=mail.example.com port=25 "crit=response not regexp '^220'"
```

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_tcp --argument "host=myserver.example.com" --argument "port=443"
```

---

## SSH Service Check

Use `check_ssh` to confirm an SSH server is up and presenting a valid banner. It
connects (default port 22), reads the greeting and requires it to start with
`SSH-`. Nothing is written to the peer, so no key exchange or authentication
takes place.

### Command

```
check_ssh host=192.168.0.10
```

### Expected output (healthy)

```
OK: 192.168.0.10:22 ok in 3ms
```

A port that answers but is not SSH yields `no_match` (CRITICAL); a closed port
yields `refused`. Use a non-standard port with `port=2222`, and tighten timing
with `"crit=time > 1000 or result != 'ok'"`.

---

## HTTP / HTTPS Endpoint Health

Use `check_http` to verify that a web endpoint is reachable, returns the expected HTTP status code, and responds within a given time.

### Command

```
check_http url=https://myapp.example.com/health
```

### Expected output (healthy)

```
OK: https://myapp.example.com/health -> 200 ok (1234B in 45ms)
'https://myapp.example.com/health_code'=200;0;200 'https://myapp.example.com/health_time'=45;5000;0
```

### Expected output (alert — HTTP 500)

```
CRITICAL: https://myapp.example.com/health -> 500 http_500 (512B in 23ms)
```

### Expected output (alert — connection failure)

```
CRITICAL: https://myapp.example.com/health -> 0 error: resolve: host not found
```

### Common scenarios

**Check an HTTPS endpoint with explicit host and path components:**

```
check_http protocol=https host=myapp.example.com port=443 path=/health
```

**Require a specific string in the response body:**

```
check_http url=https://myapp.example.com/health expected-body="status: ok"
```

If the string is absent, the check goes CRITICAL.

**Check multiple URLs:**

```
check_http url=https://myapp.example.com/ url=https://myapp.example.com/api/status
```

**Tighter latency and error thresholds:**

```
check_http url=https://myapp.example.com/ timeout=10000 \
  "warn=time > 500 or code >= 400" \
  "crit=time > 2000 or code >= 500 or result != 'ok'"
```

**Custom output format:**

```
check_http url=https://myapp.example.com/ \
  "top-syntax=%(status): %(list)" \
  "detail-syntax=%(url) -> %(code) in %(time)ms"
```

```
OK: https://myapp.example.com/ -> 200 in 45ms
```

**Send custom headers (e.g., for API authentication):**

```
check_http url=https://myapp.example.com/api header="Authorization: Bearer mytoken"
```

**Use HTTP Basic authentication:**

```
check_http url=https://myapp.example.com/private username=admin password=secret
```

**Use a different HTTP method / POST a body** (`post-data` implies POST):

```
check_http url=https://myapp.example.com/health method=HEAD
check_http url=https://myapp.example.com/api method=POST post-data="ping=1" content-type="application/json"
```

**Follow redirects** (by default a 3xx is reported as-is; `onredirect=follow` chases the `Location`):

```
check_http url=http://myapp.example.com/ onredirect=follow
```

**Accept a set of status codes / match the body with a regex** (via the `code` and `body` keywords):

```
check_http url=https://myapp.example.com/ "warn=code not in (200,301,302)" "crit=body not regexp 'Welcome'"
```

**Warn before the TLS certificate expires** (`ssl_expiry_days`):

```
check_http url=https://myapp.example.com/ "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 7"
```

**Verify the certificate and control the TLS version.** By default `check_http`
**verifies** the server certificate against the system CA bundle (`verify=peer`)
and requires **TLS 1.2 or newer** (`tls-version=tlsv1.2+`), so a handshake
against an untrusted or expired chain fails the check. Point `ca=` at a private
CA, relax verification for a self-signed endpoint, override the SNI/verification
hostname, or tighten the negotiated protocol:

```
check_http url=https://internal.example.com/  ca=/etc/ssl/private/corp-ca.pem
check_http url=https://self-signed.example.com/  verify=none
check_http url=https://myapp.example.com/  tls-version=tlsv1.3   ; CRITICAL if the server can't negotiate TLS 1.3
check_http url=https://vhost.example.com/  sni=canonical.example.com
```

Accepted `tls-version` values are `tlsv1.0`, `tlsv1.1`, `tlsv1.2`, `tlsv1.2+`
(default), `tlsv1.3` and `sslv3`. Combine with `ssl_expiry_days` to catch an
expiring **and** an untrusted or weak-protocol certificate in one check.

**Assert on values inside a JSON response body** (`--json-path`). Each
`alias:path` extracts a value from the parsed JSON and turns it into a filter
keyword you can threshold on (and it is emitted as perfdata). Numeric path
segments index into arrays; single-quote a segment that itself contains a dot:

```
check_http url=https://api.example.com/health "json-path=qlen:data.queue.length" "crit=qlen > 100"
check_http url=https://api.example.com/health "json-path=st:status" "crit=st != 'ok'"
check_http url=https://api.example.com/health "json-path=err:metrics.error_rate" "warn=err > 0.01" "crit=err > 0.05"
check_http url=https://api.example.com/health "json-path=first:items.0.name" "json-path=cfg:'a.b'.c"
```

Numeric values keep full precision (so `err > 0.05` works on `0.06`), string
values compare and render as strings, and booleans read as `1`/`0`. A path that
is missing (or a body that is not JSON) leaves the alias empty rather than
failing the check. Multiple `--json-path` options can be combined with the usual
`warning=`/`critical=` boolean expressions.

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_http --argument "url=https://myapp.example.com/health"
```

<!-- @formatter:off -->
!!! warning
    Passing arguments via NRPE requires `allow arguments = true` in the NRPE server configuration. See [NRPE security](nrpe.md).
<!-- @formatter:on -->

---

## DNS Resolution Check

Use `check_dns` to verify that a hostname resolves to the expected IP address.

### Command

```
check_dns host=myapp.example.com
```

**Require a specific resolution result:**

```
check_dns host=myapp.example.com server=8.8.8.8 "crit=address != '93.184.216.34'"
```

**Query a specific record type** (`A`, `AAAA`, `MX`, `TXT`, `CNAME`, `NS`, `SOA`, `PTR`):

```
check_dns host=example.com type=MX server=8.8.8.8
OK: example.com -> 10 smtp.example.com (1) in 9ms [ok]
```

A/AAAA lookups without `server=` use the system resolver; any other record type,
or an explicit `server=`, uses a direct DNS-over-UDP query. Add `port=` for a
non-standard resolver port and `norecursion=true` to query an authoritative
server directly.

---

## Remote Agent Availability

Use `check_nsclient_web_online` from a central host to confirm that a **remote**
NSClient++ agent's REST API is online, or to run a check on it and pass the result 
through.

### Command

```
check_nsclient_web_online url=https://192.168.0.10:8443 password=secret
```

### Expected output

```
OK: REST API reachable on https://192.168.0.10:8443
```

Run a remote check and mirror its result locally with `command=`:

```
check_nsclient_web_online url=https://192.168.0.10:8443 password=secret command=check_cpu
```

A wrong password reports `Authentication failed (HTTP 403)`; an unreachable
agent is CRITICAL. The remote certificate is not verified by default
(`verify=none`) since agents usually present a self-signed certificate — set
`verify=peer` with `ca=` to enforce it.

---

## Configuration Example

```ini
[/modules]
CheckNet   = enabled
NRPEServer = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1
port          = 5666
allow arguments = true   ; needed to pass host/url as arguments
```

---

## Next Steps

- [Windows Server Health](windows-server-health.md) — combine network checks with system health checks
- [External Scripts](external-scripts.md) — run custom network checks via scripts
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — understand filter and threshold expressions
- [Reference: CheckNet](../reference/check/CheckNet.md) — full command reference
