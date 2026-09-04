---
icon: "🔒"
modules: [SMTPClient]
---
**SMTP submissions now verify the server certificate against the agent's
CA bundle.** `SMTPClient` relied on OpenSSL's built-in default verify paths,
which on Windows do not include the Windows certificate store — so the
default `security=starttls` failed verification against Gmail, Microsoft 365
and every other public provider there, and `insecure-skip-verify` was the
only way through. A new **`ca`** target setting (and `--ca` argument)
defaults to `${ca-path}`, the same trusted bundle the other TLS clients use:
the distribution's CA store on unix, the exported Windows ROOT store on
Windows. **If a Windows SMTP target was only working because you set
`insecure-skip-verify = true`, remove it and retry** — verification should
now succeed. For an internal relay with a private CA, point `ca` at that
bundle instead of waiving verification. Set `ca = none` to restore the old
behaviour. A bundle that cannot be loaded now fails the submission with a
message naming the file, rather than failing the handshake later with an
unrelated-looking issuer error. A target that names no `ca` at all — a
one-shot command line, or a default target — falls back to the same bundle,
resolved once at module load, so no submission path is left on OpenSSL's
built-in verify paths by accident.
