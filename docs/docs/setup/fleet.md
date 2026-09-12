# Central management with NSClient Fleet

**Goal:** run an [NSClient Fleet](https://github.com/mickem/nsclient-fleet-server) server as
a container, enroll NSClient++ agents against it, and manage their configuration centrally
instead of editing `nsclient.ini` on every machine.

This is the whole round trip in one page: the server, the trust between the two, the
enrollment, and what actually changes on an agent afterwards.

---

## What you get

The fleet server is a control plane for NSClient++ installations. Agents pull from it;
it never pushes.

```
      operator's browser                      NSClient++ agent
              │                                      │
              │ HTTPS                                │ mTLS, ALPN nsclient-fleet/1
              ▼                                      ▼
      ┌───────────────────────────────────────────────────┐
      │  fleet server  (one container, one port)          │
      │    groups → bundles → desired state per host      │
      └───────────────────────────────────────────────────┘
```

An enrolled agent polls for its desired configuration, downloads the bundles assigned to
it, renders them into a `fleet.ini` that its own `nsclient.ini` includes, and reports back.
There is no module to enable and no inbound port to open on the agent.

<!-- @formatter:off -->
!!! note "Two separate products"
    NSClient++ is the agent; NSClient Fleet is the server, and lives in
    [its own repository](https://github.com/mickem/nsclient-fleet-server). This page covers
    the agent side in full and the server side as far as you need it to get going — for
    production deployment of the server (backups, sizing, Let's Encrypt, multi-tenant) see
    that repository's `docs/`.
<!-- @formatter:on -->

---

## Step 1 — Run the fleet server

One container. The database, the web UI and TLS termination are all inside the binary, so
there is nothing else to stand up.

```bash
MASTER_KEY=$(openssl rand -base64 32)

docker run -d --name nsclient-fleet --restart unless-stopped \
  -e MASTER_KEY="$MASTER_KEY" \
  -e BASE_URL="https://fleet.example.internal:8443" \
  -e ON_PREM=true \
  -e ON_PREM_ADMIN_EMAIL=admin@example.internal \
  -e ON_PREM_ADMIN_PASSWORD='a strong password' \
  -v fleet-data:/data \
  -p 8443:8443 \
  ghcr.io/mickem/nsclient-fleet:latest
```

<!-- @formatter:off -->
!!! danger "Two things here cannot be recovered"
    **`MASTER_KEY`** encrypts the tenant CA in the database. Start the server with a
    different one and everything it protects is unreadable — store it in a password
    manager now, not later, and not only inside `fleet-data`.

    **`fleet-data`** holds `mtls-server.key`, the certificate every enrolled agent pins.
    Lose the volume and every agent is stranded: renewal itself needs a working mTLS
    session, so they cannot recover on their own and must be re-enrolled by hand.
<!-- @formatter:on -->

`BASE_URL` is the address **agents** will dial, and its hostname ends up inside the
certificate they pin — so it has to be the name they can resolve, not `localhost`. Getting
it wrong is cheap to fix now and expensive after the first host enrolls.

Check it came up:

```bash
docker logs nsclient-fleet | tail -5
curl -k https://fleet.example.internal:8443/healthz    # → OK
```

Then open `https://fleet.example.internal:8443/` and sign in with the address and password
you passed. Your browser will warn about the certificate — by default the server issues
itself a self-signed one. That warning is also the subject of the next step, which matters
for a reason that has nothing to do with browsers.

## Step 2 — Give the agents something to trust

This is the step that catches people, so it gets its own section.

**Enrollment is the one call an agent makes over ordinary HTTPS.** Everything afterwards is
mTLS against a certificate the agent pinned during enrollment, but that first call has to
verify the server the normal way — against the machine's own CA bundle
(`/etc/ssl/certs/ca-certificates.crt` on Debian, the Windows ROOT store). A self-signed
certificate is not in there, so enrollment fails:

```
Enrolling with https://fleet.example.internal:8443...
Enrollment failed: Failed to contact https://fleet.example.internal:8443/enroll/v1:
  Failed to connect to fleet.example.internal:8443: certificate verify failed (SSL routines)
```

That is the error, verbatim, and it is not about the token.

Take the server's certificate out of the container and hand it to the agents as a trust
anchor:

```bash
docker cp nsclient-fleet:/data/web-server.crt ./fleet-ca.pem
openssl x509 -in fleet-ca.pem -noout -subject -ext subjectAltName
```

Copy that one file to each machine you are going to enroll — it is a public certificate,
so it needs no special handling in transit. Then pass it as `--ca`:

```bash
sudo install -D -m 0644 fleet-ca.pem /etc/nsclient/security/fleet-ca.pem
sudo nscp enroll --server https://fleet.example.internal:8443 \
                 --token <bootstrap-token> \
                 --ca /etc/nsclient/security/fleet-ca.pem
```

If the fleet server uses a certificate from your own internal CA, or from Let's Encrypt,
this step disappears: point `--ca` at your CA bundle, or drop the flag entirely for a
publicly trusted certificate.

<!-- @formatter:off -->
!!! warning "The lab shortcut, and why `--insecure` alone is not it"
    `--insecure` on its own does **not** skip verification over `https://` — the CA still
    defaults to the platform bundle and you get exactly the same failure. Turning
    verification off takes both flags:

    ```bash
    sudo nscp enroll --server https://fleet.example.internal:8443 \
                     --token <token> --verify none --insecure
    ```

    Only on a network you trust. The enrollment response carries the certificate this agent
    pins from then on *and* the key it trusts for executable bundles, so an unverified
    enrollment hands both to whoever answered — and pinning then keeps working perfectly,
    against the wrong server. The command warns when you do this.
<!-- @formatter:on -->

## Step 3 — Add the host on the server

In the UI: **Hosts → Add host**. You get back an install command carrying a one-time
bootstrap token, valid for an hour:

```
nscp enroll --server https://fleet.example.internal:8443 --token eyJ0eXAiOiJKV1Qi…
```

Or over the API, with an API key from **API keys**:

```bash
curl -sS -X POST -H "Authorization: Bearer nsk_…" \
  https://fleet.example.internal:8443/api/hosts
```

The host appears immediately as **awaiting enrollment** and stays there until an agent
uses the token.

<!-- @formatter:off -->
!!! note "The token is one-time and is burned on first use"
    A second attempt with the same token is refused:

    ```
    Enrollment failed: Enrollment rejected (401): the bootstrap token is invalid,
    expired or already used - generate a new install command on the fleet server
    ```

    Note *which* failures burn it. A TLS failure (Step 2) happens before the token is ever
    sent, so that token is still good — fix the `--ca` and run the same command again.
<!-- @formatter:on -->

## Step 4 — Enroll the agent

NSClient++ must already be installed — see [Installation](installing.md).

=== "Linux"

    ```bash
    sudo nscp enroll --server https://fleet.example.internal:8443 \
                     --token <bootstrap-token> \
                     --ca /etc/nsclient/security/fleet-ca.pem
    sudo systemctl restart nsclient
    ```

    Run it as root: enrollment writes the identity and then hands ownership to the
    unprivileged service account. If it cannot, it says so and prints the `chown` to run —
    skipping that leaves a healthy-looking agent that never appears in the fleet.

=== "Windows"

    At install time, as MSI properties:

    ```
    msiexec /qn /i NSCP-<version>-x64.msi ^
      FLEET_SERVER=https://fleet.example.internal:8443 ^
      FLEET_TOKEN=<bootstrap-token> ^
      FLEET_CA=C:\path\to\fleet-ca.pem
    ```

    Or afterwards, from an elevated prompt:

    ```
    nscp enroll --server https://fleet.example.internal:8443 --token <bootstrap-token> --ca C:\path\to\fleet-ca.pem
    ```

    See [Enrolling with a fleet server](installing.md#enrolling-with-a-fleet-server) for
    the MSI properties in full, including what happens on upgrade and uninstall.

A successful enrollment says so and tells you where it put things:

```
Enrolling with https://fleet.example.internal:8443...
Enrollment successful.
  Identity stored in: /var/lib/nsclient/security/agent-state.json
  Agent API (mTLS):   https://fleet.example.internal:8443
  Fleet configuration sync starts on the next service start.
```

### What enrollment changed

Two things, and nothing else:

| | |
| --- | --- |
| `/var/lib/nsclient/security/agent-state.json` | This host's private key, its issued client certificate, the server certificate it now pins, and the key that authorises bundles |
| `[/includes] fleet = ${fleet-folder}/fleet.ini` in `nsclient.ini` | The one line that makes the agent read what the server sends it |

The include is written as the `${fleet-folder}` token rather than a resolved path, so it
follows the [file layout](../concepts/file-layout.md) rather than pinning it. No module is
enabled: the sync starts automatically whenever `agent-state.json` exists.

### Confirm it worked

On the agent:

```bash
sudo journalctl -u nsclient | grep -i fleet
```

```
fleet Fleet configuration sync started (manifest: /var/lib/nsclient/security/agent-state.json)
fleet Applied fleet configuration 44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a
```

On the server, the host moves off *awaiting enrollment* to a real status — **in sync** once
it has applied what it was given. Its hostname and OS are filled in from what the agent
reported, which replaces whatever you typed when adding it.

## Step 5 — Send it some configuration

Configuration travels as **bundles** — signed archives holding an INI fragment and,
optionally, scripts. Bundles are assigned to **groups**, and a group selects hosts by tag.

In the UI:

1. **Bundles → New bundle.** Give it a name and a version, and write the fragment as
   ordinary INI:

   ```ini
   [/modules]
   CheckSystem = enabled
   CheckDisk = enabled
   CheckHelpers = enabled

   [/settings/default]
   allowed hosts = 10.0.0.0/8

   [/settings/NRPE/server]
   port = 5666
   ```

2. **Groups → New group.** A group with no clauses matches every host in the tenant, which
   is the right starting point; narrow it later with tag selectors.

3. Assign the bundle to the group.

Within one poll interval the agent picks it up, and `fleet.ini` is rewritten:

```bash
cat /var/lib/nsclient/fleet/fleet.ini
```

```ini
; Managed by the fleet sync - DO NOT EDIT, changes are overwritten on the next sync.

[/modules]
CheckDisk=enabled
CheckHelpers=enabled
CheckSystem=enabled

[/settings/NRPE/server]
port=5666

[/settings/default]
allowed hosts=10.0.0.0/8
```

From there it is ordinary configuration — the agent reads it through the include:

```bash
sudo nscp settings --path /settings/default --key "allowed hosts" --show
10.0.0.0/8
```

<!-- @formatter:off -->
!!! important "Local settings win"
    A key set in the host's own `nsclient.ini` keeps its local value even when the fleet
    server sends a different one. That is a legitimate way to run — pin something locally,
    manage the rest centrally — but it is a genuine surprise when a fleet setting appears
    to have no effect. `nscp enroll` warns at enrollment time when the host already has
    local configuration, and the server is told that local overrides exist (never what they
    are).

    So: if a fleet-managed value is not taking effect, look in `nsclient.ini` before you
    look at the server.
<!-- @formatter:on -->

Never edit `fleet.ini` itself. It is rewritten wholesale on the next sync, which is exactly
what the banner at the top of it says.

## Step 6 — Living with it

**Certificates renew themselves.** The client certificate an agent gets is short-lived and
the agent renews it over its existing mTLS session, before expiry. Nothing to schedule.

**Re-enrolling.** `agent-state.json` is this host's identity. To move a host to a different
fleet server, or to recover one whose identity was lost, add it on the server again and
enroll with `--force`:

```bash
sudo nscp enroll --server https://fleet.example.internal:8443 --token <new-token> \
                 --ca /etc/nsclient/security/fleet-ca.pem --force
```

**Leaving the fleet.** Delete `agent-state.json` and remove the `[/includes] fleet` line.
The agent goes back to whatever is in its own `nsclient.ini`; nothing else is touched.

**Upgrading the server.** Pull the new image and recreate the container with the *same*
`MASTER_KEY` and the *same* volume. Agents see the same server with the same pinned
certificate and nothing re-enrolls.

---

## Troubleshooting

**`certificate verify failed (SSL routines)` during enrollment.** Step 2 — the agent does
not trust the fleet server's certificate. The token is untouched; fix `--ca` and retry.

**`Failed to load CA <path>: load_verify_file: No such file or directory`.** The `--ca`
path is wrong or unreadable by the user running the command. It is read before the verify
mode is considered, so this fails even with `--verify none`.

**`the bootstrap token is invalid, expired or already used`.** Tokens last an hour and are
one-time. Generate a new install command.

**Enrollment succeeds, but the host never leaves *awaiting enrollment*.** The sync starts
at the next service start — `sudo systemctl restart nsclient`. If it still does not appear,
check that nothing is terminating TLS between agent and server: agents dial with ALPN
`nsclient-fleet/1` and pin the server's certificate, so an inspecting proxy or a
TLS-terminating load balancer breaks both.

**`Desired-state poll rate limited` in the agent log.** Harmless. The server enforces a
minimum poll interval per tier and the agent asked early; it backs off and retries. It is
only a problem if you see nothing else.

**A fleet setting has no effect.** Something local is overriding it — see the note in
Step 5.

**The agent applied a configuration you did not expect.** `fleet.ini` shows exactly what
arrived, and the server's audit log shows who changed what. Section names come from the
bundle's structure, so a stray leading slash in a hand-built bundle produces `[//modules]`
rather than `[/modules]` and silently matches nothing — worth a glance if a module refuses
to load.

---

## Next steps

- [Installation](installing.md) — installing NSClient++ itself, on Windows and Linux.
- [File layout](../concepts/file-layout.md) — where `agent-state.json`, `fleet.ini` and the
  bundle cache live, and why they are under `/var` on Linux.
- [Securing NSClient++](securing.md) — hardening the agent's own listeners, which the fleet
  server does not touch.
- [nsclient-fleet-server](https://github.com/mickem/nsclient-fleet-server) — running the
  server properly: backups, certificates, sizing, multi-tenant.
