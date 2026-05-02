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

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_tcp --argument "host=myserver.example.com" --argument "port=443"
```

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

**Via NRPE:**

```
check_nrpe -H <agent-ip> -c check_http --argument "url=https://myapp.example.com/health"
```

!!! warning
    Passing arguments via NRPE requires `allow arguments = true` in the NRPE server configuration. See [NRPE security](nrpe.md).

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
- [Checks In Depth: Filters](../checks-in-depth/index.md#4-filters-choosing-what-to-check) — understand filter and threshold expressions
- [Reference: CheckNet](../reference/check/CheckNet.md) — full command reference
