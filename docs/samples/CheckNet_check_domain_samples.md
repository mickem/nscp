#### Check a registered domain

```text
check_domain domain=example.com
OK: example.com expires in 318d (2027-08-13T04:00:00Z, registry, rdap:https://rdap.verisign.com/com/v1/domain/example.com)
'example.com'=318d;30;10
```

For a real deployment, replace `example.com` with a domain you manage. The
default thresholds are warning below 30 days and critical below 10 days.

#### Override the endpoint and thresholds

These outputs were captured on 2026-09-28 against a local HTTPS RDAP fixture
reporting an expiration of `2030-01-01T00:00:00Z`. The fixture served the domain
`example.com`; these are not claims about its public registration. The CA file
path in the command has been shortened for readability.

```text
check_domain domain=example.com "rdap-url=https://localhost:52513/domain/{domain}" ca=ca.crt
OK: example.com expires in 1190d (2030-01-01T00:00:00Z, registry, rdap:https://localhost:52513/domain/example.com)|'example.com'=1190d;30;10

check_domain domain=example.com "rdap-url=https://localhost:52513/domain/{domain}" ca=ca.crt "critical=expires_in < 2000"
CRITICAL: example.com expires in 1190d (2030-01-01T00:00:00Z, registry, rdap:https://localhost:52513/domain/example.com)|'example.com'=1190d;30;2000
```

The source and expiration type distinguish the registry's expiry date from a
registrar's renewal deadline. Prefer the registrar date when it is available;
the check selects it automatically.

#### Enable WHOIS fallback

```text
check_domain domain=your-domain.example whois-fallback=true whois-server=whois.your-provider.example
```

Replace both names with your registered domain and its provider's WHOIS server.
WHOIS is only queried after RDAP fails or provides no usable expiration.
The output's source starts with `whois:` when fallback supplied the date.

#### Invalid input returns UNKNOWN

Captured output (process exit code 3):

```text
nscp client --module CheckNet --boot --query check_domain domain=https://example.com
Domain lookup failed: Invalid domain name; use ASCII or punycode
```
