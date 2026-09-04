---
icon: "🔒"
modules: [WEBServer]
---
**WEB server security hardening.** A review of the `WEBServer`
module produced several defense-in-depth fixes (session tokens now come from
the OpenSSL CSPRNG, cookie-name matching requires a name boundary, the
installer refuses an HTTPS→HTTP redirect, and the `legacy` grant's startup
warning now names `/settings/query.pb`). The default install needs no
action. Two changes touch observable behaviour: a script or module **name
that begins with `-` is now rejected** (rename it; interior dashes are
fine), and the legacy **`POST /auth/logout` route now enforces `allowed
hosts`** like the rest of the API (a caller outside the perimeter gets 403).
A third only bites on a broken system: if the OpenSSL CSPRNG fails, the
server now **refuses to issue a session token** (HTTP 500, logged as
`SECURITY:`) rather than falling back to a weaker generator.
See the [security notice](../security/notices.md#web-server-security-hardening).
