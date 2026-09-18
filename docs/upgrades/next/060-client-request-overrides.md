---
icon: "🔒"
modules: [NRDPClient, NRPEClient, NSCAClient, NSCANgClient, IcingaClient, SMTPClient]
action: conditional
---
**A request may no longer weaken the transport security of a credentialed
target, or re-address `submit_smtp` mail.** The client override guard now covers
the keys that decide *how* a connection is protected — `verify`, `insecure`,
`insecure-skip-verify`, `no-psk`, `security`, `ssl`, `no ssl`, `tls-version`,
`ca`, `certificate`, `certificate-key`, `allowed-ciphers`, `dh` — and, for SMTP,
`recipient` and `sender`. Nothing to do unless callers pass one of those to a
target that carries a password or token; repeating what the target configured is
still fine, and the remedies are the same as for `host=`: supply the credentials
with the request, configure the variant as its own target and select it with
`target=`, or set `allow host override = true`. The addressing keys have their
own, narrower opt-in, `allow recipient override = true`. `payload-length` /
`buffer-length` are clamped to what each protocol accepts, 65536 for NSCA and
1 MiB for NRPE, so an absurd value can no longer allocate gigabytes while every
supported payload size keeps working. A failed
CA load reports a generic message with the path and reason in the log. See the
[security notice](../security/notices.md#outbound-clients-transport-and-recipient-overrides-ca-error-text-payload-length).
