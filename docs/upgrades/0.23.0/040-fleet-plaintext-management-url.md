---
icon: "🔒"
modules: [core]
action: conditional
---
**A fleet management url that is not `https://` is now refused.** Nothing to do
for a host enrolled against an https fleet server. The `mtls_url` in the
enrollment manifest is where desired state and signed bundles come from, and on
a plain socket neither the agent's client certificate nor the pinned server
certificate does anything — the whole management channel ran unauthenticated,
silently. Enrollment now refuses a non-https management url, the sync loop
refuses to start on a stored one, and the http client refuses to attach a
client certificate or a pinned CA to a non-TLS transport at all. A url with no
scheme counts as plaintext, because it is opened on a plain socket just the
same. Where plaintext is what you want, `nscp enroll --insecure` records the
decision in the manifest and `[tls] allow plaintext = true` in `boot.ini`
allows it for an older manifest; the sync then logs `INSECURE` on every start.
See the
[security notice](../security/notices.md#fleet-plaintext-management-urls-refused-and-enrollment-no-longer-follows-planted-symlinks).
