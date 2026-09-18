#### About `check_http`

`check_http` makes an HTTP or HTTPS request and checks the status code,
response time, body size and body content. One record is returned per URL, so
`url=` can be repeated to probe several endpoints in one check.

By default it warns above 5000 ms and goes critical when the status code is
outside 200–399 or `result` is not `ok`. The target can be given either as a
full `url=`, or assembled from `host=` / `port=` / `path=` / `protocol=` (with
`ssl=true` as a shorthand for `protocol=https`).

##### What `result` distinguishes

`code` alone does not tell you whether the check is happy, because several
failure modes never produce a status code at all. `result` is the keyword that
carries the verdict: `ok`, `no_match` when `expected-body=` was given and the
substring is absent, and an error value when the request could not be completed
(connection refused, TLS failure, timeout). A body-content mismatch is the case
worth wiring up deliberately: an application that has failed into a friendly
error page still answers `200`, and only `expected-body=` catches it.

##### TLS

Certificates are **verified by default** (`verify=peer`) against the system CA
bundle, or a bundle you point `ca=` at. `tls-version=` defaults to `tlsv1.2+`
and `sni=` overrides the name used for SNI and hostname verification when it
differs from the URL host (a vhost behind a shared address, or a probe aimed at
a specific backend by IP).

`ssl_expiry_days` reports how long the presented certificate is still valid, so
one check can cover both reachability and expiry:

```
check_http url=https://example.com "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 7"
```

On a plain `http` URL there is no certificate, so `ssl_expiry_days` renders as
`no certificate` and **every numeric comparison against it is false** — a
`< 30` threshold quietly does not fire rather than alerting on every plain-HTTP
target. Test for the absence explicitly with
`ssl_expiry_days = 'no certificate'`.

##### Redirects, auth and methods

`onredirect=` decides what a 3xx means: `ok` (the default — the redirect itself
is the expected answer), `follow` (chase it, up to `max-redirs`, default 15),
`warning` or `critical`. `username=` / `password=` add HTTP Basic
authentication, `method=` selects the verb, and `post-data=` sends a body
(implying POST unless `method=` says otherwise). `header=` is repeatable for
arbitrary request headers.

##### Checking a JSON API

`json-path=` extracts a value from a JSON response body and exposes it as a
filter keyword, written as `alias:dotted.path` and repeatable. Numeric segments
index into arrays, and a segment containing a literal dot can be single-quoted.
This turns a health endpoint into a real check rather than a 200-or-not probe:

```
check_http url=https://api.example.com/health json-path=qlen:data.queue.length "crit=qlen > 100"
```

##### Certificate identity and required names

Beyond `ssl_expiry_days`, an https check reports who the certificate is for and
who issued it, using the same keyword vocabulary as `check_tcp` — a filter
written for one works verbatim against the other:

| Keyword | Value |
| --- | --- |
| `cert_subject` | Subject as an RFC 2253 string, e.g. `CN=www.example.com,O=Acme` |
| `cert_cn` | The subject's commonName only |
| `cert_issuer` / `cert_issuer_cn` | Issuer, in full and as its commonName |
| `cert_sans` | subjectAltName entries, comma separated (`DNS:host`, `IP:addr`) |
| `cert_self_signed` | True when subject equals issuer |
| `cert_verify` | OpenSSL's verdict on the chain: `ok`, or why it did not verify |
| `missing_sans` | Names required via `sans=` that the certificate does not cover |

`sans=` takes a comma separated list of names the certificate must cover
through subjectAltName. A missing name sets `result` to `san_missing` — which
the default `critical` filter already alerts on — and names it in
`missing_sans`. Wildcards follow RFC 6125: `*.example.com` covers
`www.example.com` but not `example.com` itself.

```
check_http url=https://www.example.com/ sans=example.com,www.example.com
```

All of these describe the **last** hop: with `onredirect=follow`, an https hop
followed by a plain http one reports no certificate at all, and `sans=` is
evaluated against the certificate served by the URL actually checked. A hop
that served no certificate covers no names, so a `sans=` requirement fails
there rather than passing by default.

`cert_verify` is recorded even when the chain does not verify, so a check can
report *why* a certificate is untrusted. It is not an authentication result on
its own — only a successful request under the default `verify=peer` is that.

`ca=` accepts a PEM bundle file or a hashed CA directory (`/etc/ssl/certs`).
