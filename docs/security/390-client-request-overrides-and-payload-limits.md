---
title: "Outbound clients: transport and recipient overrides, CA error text, payload length"
fixed_in: 0.22.0
severity: "Low"
modules: [NRDPClient, NRPEClient, NSCAClient, NSCANgClient, IcingaClient, SMTPClient, GraphiteClient, CheckNet]
action: conditional
---
Four findings from a review of the outbound client modules. All of them need a
caller who can already run the module's `submit_*` / `check_*` commands with
arguments — a REST user holding `queries.execute`, or an NRPE peer with
`allow arguments = true`.

#### A request could turn off certificate verification for a credentialed target

The host-override guard introduced in
[notice 160](notices.md#client-modules-a-request-could-redirect-configured-credentials-to-another-host)
protects *where* a configured credential goes. It said nothing about *how* it
gets there, and the transport keys are request options on most of these
modules: `submit_nrdp verify=none`, `submit_smtp insecure-skip-verify=true`,
`submit_nsca_ng insecure=true` all left the destination exactly as the target
configured it while removing the check that the host answering for that name is
the configured server. A DNS or on-path attacker then collected the NRDP token,
the Icinga basic-auth credentials or the SMTP AUTH password.

The guard now covers the keys that decide how the connection is protected —
`verify mode`, `insecure`, `insecure-skip-verify`, `use psk`, `security`, `ssl`,
`no ssl`, `tls version`, `ca`, `certificate`, `certificate key`,
`certificate format`, `allowed ciphers`, `dh` — on the same terms as `host=`
and `proxy=`. A request that repeats what the target configured changes
nothing and is allowed; one that brings its own credentials is unaffected, as
is a target with `allow host override = true`.

#### `submit_smtp` could address mail as the caller liked

`recipient=` and `sender=` are request options, and the destination — the
submission server — was unchanged, so nothing above noticed that the caller had
chosen both ends of the message. The agent's authenticated mailbox was
therefore usable as a relay by anyone who could run `submit_smtp`. Header
injection was already closed
([notice 060](notices.md#smtp-client-hardening)); this is the abuse of intended
function that was left.

Both keys are now settings-only while the target carries credentials, with
their own opt-in: `allow recipient override = true` on the SMTP target, which
does not also open the destination the way `allow host override` does.

#### A request-supplied `ca=` reported whether a path existed

`ca=` names a file the agent opens, and the OpenSSL reason for a failed load —
"No such file or directory", "Permission denied", "no start line" — travelled
back in the check result. That is a file-existence and readability oracle over
the whole filesystem, answered with the agent's privileges.

The reason now goes to the agent log and the caller gets a generic message.
This applies to the HTTP clients (NRDP, Icinga), the raw-socket clients
(NSCA-NG, Graphite) and `check_tcp`. The guard above closes the same path for a
credentialed target outright.

#### A caller-chosen payload length allocated gigabytes

`payload-length` / `buffer-length` are request options on the NSCA and NRPE
clients and had no upper bound below `INT_MAX`. One submission naming
2147483647 allocated about 2 GB per payload — CSPRNG output for NSCA, a zeroed
packet for NRPE. On 32-bit builds that is an allocation failure per call; on
64-bit, memory exhaustion from a few concurrent requests. The server side
already refused such lengths. Each client now clamps to what its own protocol
accepts — 65536 for NSCA, and 1 MiB for NRPE, the ceiling its v3/v4 decoder
already enforces — and up from a minimum of 16, logging once when it does. The
bound exists to stop a multi-gigabyte allocation, not to shrink either
protocol: a 1 MiB NRPE payload is a supported configuration and still works.

**What to do:** nothing on a default install. If callers pass `verify=`,
`insecure=`, `ca=`, `recipient=` or `sender=` to a target that carries a
password or token, they will now be refused: pass the credentials with the
request, configure the variant as its own target and select it with `target=`,
or set `allow host override = true` (or `allow recipient override = true` for
the addressing keys alone).
