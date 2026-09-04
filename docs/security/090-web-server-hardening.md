---
title: "WEB server security hardening"
fixed_in: 0.18.0
severity: "Low–Medium"
modules: [WEBServer]
---
A focused hardening pass over the `WEBServer` module (REST API + web UI)
produced a set of defense-in-depth fixes. None is a confirmed remote
vulnerability in a default configuration; they close latent gaps and
consistency issues.

- **Session tokens are drawn from the OpenSSL CSPRNG.** `token_store::generate_token`
  minted bearer session tokens (and generated admin passwords) with
  `std::random_device`, which the C++ standard does not require to be
  non-deterministic — some toolchains have historically implemented it as a
  fixed-seed PRNG, which would make tokens predictable. Tokens now come from
  `RAND_bytes()` (the same CSPRNG already used for password salts), with
  rejection sampling to keep the alphabet unbiased. A build *without* OpenSSL
  still uses `std::random_device` (it cannot serve TLS either); a build that
  has the CSPRNG never silently substitutes the weaker source — if
  `RAND_bytes()` fails, **no token or generated password is issued at all**
  and the request fails with a 500 (`nscp web add-user` / `nscp web install`
  report an RNG failure). Refusing is the correct outcome for a bearer
  credential, but it does mean a broken OpenSSL RNG blocks logins rather than
  degrading them; the failure is logged as a `SECURITY:` error.
- **Cookie name matching now requires a name boundary.** The bundled
  `mg_get_cookie` matched a requested cookie name as a substring, so a
  client cookie named `eviltoken` could satisfy a lookup for `token`. It now
  requires the match to start at a cookie boundary. The WEBServer module
  authenticates from headers, not request cookies, so this path was not used
  for authentication — the fix hardens the exported helper regardless.
- **The legacy `/auth/logout` route enforces the allowed-hosts perimeter.**
  Its sibling `/auth/token` already called `is_allowed()`; logout did not, so
  a host outside `allowed hosts` could revoke an observed token with a
  spoofed `User-Agent`. It now applies the same check.
- **Script / module names may no longer begin with `-`.** A leading dash could
  be mistaken for an option flag when the name is later passed as an argument
  value; interior dashes (`check-disk`) are unchanged.
- **The web-UI installer refuses an HTTPS→HTTP redirect.** The bundle's
  integrity rests on the TLS channel (the SHA-256 manifest is fetched over the
  same connection), so a redirect chain that began on `https://` is no longer
  allowed to drop to cleartext. A protocol-relative `Location` (`//host/path`)
  is now resolved as such, inheriting the current scheme, instead of being
  glued onto the current origin as a path — which both mis-resolved the
  redirect and hid it from the downgrade check. An operator who explicitly
  passes an `http://` `--url` is unaffected.
- **The `legacy` grant's startup warning now names `/settings/query.pb`.** In
  addition to the query/RCE routes it already flagged, the `SECURITY` warning
  now states that `legacy` also unlocks `POST /settings/query.pb`, which reads
  settings *without* the redaction the `/api/v2/settings` endpoints apply and
  can write settings — without holding `settings.get`/`settings.put`. This is
  a documentation fix; the endpoint's behaviour is unchanged, and `legacy` was
  already an RCE-equivalent, trusted-only grant.

**What to do:** nothing required for the default install. If you have a custom
script/module name that begins with `-`, rename it. If a client outside
`allowed hosts` relied on the legacy `/auth/logout` route, move it inside the
perimeter. Review any role that grants `legacy` — it confers unredacted
settings read and settings write in addition to command execution.
