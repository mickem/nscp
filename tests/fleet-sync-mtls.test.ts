/**
 * Real-mTLS regression test for the core fleet sync loop.
 *
 * The plain-http suite (fleet-sync.test.ts) exercises the protocol;
 * this one exercises the TLS layer that broke in the field: the agent must
 * actually PRESENT its client certificate during the handshake. (OpenSSL's
 * SSL_new() copies certificate state out of the SSL_CTX at stream-creation
 * time, so an identity loaded into the context after the stream exists is
 * silently dropped — the server then sees no certificate at all and answers
 * "tlsv13 alert certificate required".)
 *
 * Setup mirrors production: enrollment over plain HTTP returns an mtls_url
 * pointing at an https server with `requestCert: true, rejectUnauthorized:
 * true` and the tenant CA as the only client trust — exactly as strict as the
 * rustls fleet server. The agent's Ed25519 CSR is signed into a real client
 * certificate with the openssl CLI (node-forge cannot issue certs for
 * Ed25519 keys); the suite skips with a warning when openssl is unavailable.
 */
import http from "http";
import https from "https";
import tls from "tls";
import { AddressInfo } from "net";
import crypto from "crypto";
import execa from "execa";
import fs from "fs";
import os from "os";
import path from "path";
import forge from "node-forge";
import { NscpInstance, generateCertChain, CertBundle } from "@fixtures/index";

jest.setTimeout(180_000);

function findOpenssl(): string | undefined {
  for (const candidate of [process.env.NSCP_OPENSSL_BIN, "openssl"]) {
    if (!candidate) continue;
    try {
      execa.sync(candidate, ["version"]);
      return candidate;
    } catch {
      /* keep looking */
    }
  }
  return undefined;
}

const opensslBin = findOpenssl();
const describeMtls = opensslBin ? describe : describe.skip;
if (!opensslBin) {
  console.warn("[fleet-sync-mtls] openssl CLI not found (set NSCP_OPENSSL_BIN); skipping real-mTLS suite");
}

/** A self-signed server certificate (SAN localhost/127.0.0.1) — the agent
 * pins this exact cert as its only trust root, like production. */
function makeSelfSignedServerCert(): { certPem: string; keyPem: string } {
  const keys = forge.pki.rsa.generateKeyPair(2048);
  const cert = forge.pki.createCertificate();
  cert.publicKey = keys.publicKey;
  cert.serialNumber = "1001";
  cert.validity.notBefore = new Date(Date.now() - 3600_000);
  cert.validity.notAfter = new Date(Date.now() + 30 * 24 * 3600_000);
  const attrs = [{ name: "commonName", value: "fleet-mtls" }];
  cert.setSubject(attrs);
  cert.setIssuer(attrs);
  cert.setExtensions([
    { name: "basicConstraints", cA: false },
    { name: "keyUsage", digitalSignature: true, keyEncipherment: true },
    { name: "extKeyUsage", serverAuth: true },
    {
      name: "subjectAltName",
      altNames: [
        { type: 2, value: "localhost" },
        { type: 7, ip: "127.0.0.1" },
      ],
    },
  ] as unknown as Parameters<typeof cert.setExtensions>[0]);
  cert.sign(keys.privateKey, forge.md.sha256.create());
  return { certPem: forge.pki.certificateToPem(cert), keyPem: forge.pki.privateKeyToPem(keys.privateKey) };
}

/** Issue a client certificate for the agent's (Ed25519) CSR, signed by the tenant CA. */
async function signCsr(openssl: string, scratchDir: string, csrPem: string, ca: CertBundle["ca"]): Promise<string> {
  const csrPath = path.join(scratchDir, "agent.csr");
  const outPath = path.join(scratchDir, "agent.crt");
  fs.writeFileSync(csrPath, csrPem);
  await execa(openssl, ["x509", "-req", "-in", csrPath, "-CA", ca.certPath, "-CAkey", ca.keyPath, "-set_serial", "42", "-days", "30", "-out", outPath]);
  return fs.readFileSync(outPath, "utf8");
}

describeMtls("core fleet sync over strict mTLS", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let enrollServer: http.Server;
  let mtlsServer: https.Server;
  let enrollUrl: string;
  let mtlsUrl: string;
  /** Requests seen on the mTLS listener, with the peer certificate CN (if any). */
  let mtlsRequests: { url: string; body: any; peerCn?: string }[];
  let reportedHashes: (string | undefined)[];

  const tenantCa = () => ca;
  let ca: CertBundle["ca"];
  const serverIdentity = makeSelfSignedServerCert();
  const signingKeys = crypto.generateKeyPairSync("ed25519");

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-mtls-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    ca = generateCertChain({ outDir: path.join(workDir, "tenant-ca"), signed: {} }).ca;
    mtlsRequests = [];
    reportedHashes = [];

    // The strict mTLS endpoint: client certificate REQUIRED and verified
    // against the tenant CA only. A client that presents no certificate
    // fails the handshake - the field bug this suite pins down.
    mtlsServer = https.createServer(
      {
        key: serverIdentity.keyPem,
        cert: serverIdentity.certPem,
        requestCert: true,
        rejectUnauthorized: true,
        ca: [ca.certPem],
      },
      (req, res) => {
        let raw = "";
        req.on("data", (c) => (raw += c));
        req.on("end", () => {
          let body: any = raw;
          try {
            body = JSON.parse(raw);
          } catch {
            /* keep raw */
          }
          const peer = (req.socket as tls.TLSSocket).getPeerCertificate();
          const rawCn = peer && peer.subject ? peer.subject.CN : undefined;
          mtlsRequests.push({ url: req.url ?? "", body, peerCn: Array.isArray(rawCn) ? rawCn[0] : rawCn });

          const parsed = new URL(req.url ?? "/", "http://x");
          if (parsed.pathname === "/agent/v1/heartbeat") {
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end("{}");
          } else if (parsed.pathname === "/agent/v1/desired-state") {
            if (parsed.searchParams.get("current_hash") === "mtls-1") {
              res.writeHead(304, { "Content-Type": "application/json" });
              res.end(JSON.stringify({ next_poll_in_seconds: 1 }));
            } else {
              res.writeHead(200, { "Content-Type": "application/json" });
              res.end(JSON.stringify({ state_hash: "mtls-1", next_poll_in_seconds: 1, merged_config_json: {}, bundles: [] }));
            }
          } else if (parsed.pathname === "/agent/v1/state-report") {
            reportedHashes.push(body?.applied_state_hash);
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end("{}");
          } else {
            res.writeHead(404);
            res.end();
          }
        });
      },
    );
    await new Promise<void>((resolve) => mtlsServer.listen(0, "127.0.0.1", resolve));
    mtlsUrl = `https://127.0.0.1:${(mtlsServer.address() as AddressInfo).port}`;

    // Public enrollment endpoint: signs the agent's CSR with the tenant CA so
    // the issued certificate really chains to what the mTLS listener trusts.
    enrollServer = http.createServer((req, res) => {
      let raw = "";
      req.on("data", (c) => (raw += c));
      req.on("end", () => {
        (async () => {
          const body = JSON.parse(raw);
          const certPem = await signCsr(opensslBin!, workDir, body.csr_pem, tenantCa());
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(
            JSON.stringify({
              cert_pem: certPem,
              ca_pem: ca.certPem,
              bundle_signing_pub_pem: signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString(),
              server_url: enrollUrl,
              mtls_url: mtlsUrl,
              mtls_server_cert_pem: serverIdentity.certPem,
            }),
          );
        })().catch((err) => {
          res.writeHead(500);
          res.end(String(err));
        });
      });
    });
    await new Promise<void>((resolve) => enrollServer.listen(0, "127.0.0.1", resolve));
    enrollUrl = `http://127.0.0.1:${(enrollServer.address() as AddressInfo).port}`;
  });

  afterAll(async () => {
    await nscp.stop();
    await new Promise<void>((resolve) => enrollServer.close(() => resolve()));
    await new Promise<void>((resolve) => mtlsServer.close(() => resolve()));
  });

  async function waitFor(what: string, predicate: () => boolean, timeoutMs = 45_000): Promise<void> {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  }

  it("enrolls and receives a certificate chained to the tenant CA", async () => {
    const r = await nscp.run(["enroll", "--server", enrollUrl, "--token", "tok-mtls", "--insecure"], { allowFailure: true });
    expect(r.exitCode).toBe(0);
    const state = JSON.parse(fs.readFileSync(path.join(workDir, "security", "agent-state.json"), "utf8"));
    expect(state.cert_pem).toContain("BEGIN CERTIFICATE");
    expect(state.mtls_url).toBe(mtlsUrl);
  });

  it("completes the sync loop over strict mTLS, presenting its client certificate", async () => {
    nscp.start();

    await waitFor("a state report with the applied hash", () => reportedHashes.includes("mtls-1"));

    // Every request survived `requestCert + rejectUnauthorized` AND carried
    // the enrolled identity (CSR CN is "client").
    expect(mtlsRequests.length).toBeGreaterThan(0);
    for (const req of mtlsRequests) {
      expect(req.peerCn).toBe("client");
    }
    expect(mtlsRequests.some((r) => r.url === "/agent/v1/heartbeat")).toBe(true);

    // Steady state over mTLS too: poll with the applied hash -> 304.
    await waitFor("a 304 steady-state poll", () => mtlsRequests.some((r) => r.url === "/agent/v1/desired-state?current_hash=mtls-1"));
  });
});

/**
 * The other half of the pin: the agent connects with verify mode "none" and
 * relies entirely on `mtls_server_cert_pem` being the ONLY trusted root. If the
 * pin were ignored (or treated as "trust anything"), any host able to answer on
 * the mtls url - a DNS hijack, a proxy, a rotated-out server - could feed this
 * agent configuration. So: a server whose certificate is not the pinned one
 * must never complete a handshake, and an unusable pin must not fall back to
 * trusting the peer.
 */
describeMtls("core fleet sync rejects a server that does not match the pin", () => {
  interface PinCase {
    /** What enrollment hands out as mtls_server_cert_pem. */
    pin: (serverCert: string) => string;
    what: string;
    /**
     * Whether the agent is expected to get as far as a TLS handshake. A
     * parseable-but-wrong pin means it dials and TLS refuses the peer; a pin
     * OpenSSL cannot load at all means it never dials. Asserting which of the
     * two happened is what keeps these cases from passing for the wrong reason
     * (a crashed agent also serves no requests).
     */
    expectHandshake: boolean;
  }

  const cases: PinCase[] = [
    { what: "a different self-signed certificate", pin: () => makeSelfSignedServerCert().certPem, expectHandshake: true },
    {
      what: "an unrelated CA certificate",
      pin: () => generateCertChain({ outDir: fs.mkdtempSync(path.join(os.tmpdir(), "nscp-otherca-")), signed: {} }).ca.certPem,
      expectHandshake: true,
    },
    { what: "a garbage pin", pin: () => "-----BEGIN CERTIFICATE-----\nnot a certificate\n-----END CERTIFICATE-----\n", expectHandshake: false },
  ];

  for (const item of cases) {
    it(`refuses to talk to the fleet server when the pin is ${item.what}`, async () => {
      const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-pin-"));
      const nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
      const ca = generateCertChain({ outDir: path.join(workDir, "tenant-ca"), signed: {} }).ca;
      const serverIdentity = makeSelfSignedServerCert();
      const signingKeys = crypto.generateKeyPairSync("ed25519");
      /** Requests that made it through TLS - must stay empty. */
      const served: string[] = [];
      /** Handshakes the server rejected or the client aborted. */
      let handshakeFailures = 0;

      const mtlsServer = https.createServer(
        { key: serverIdentity.keyPem, cert: serverIdentity.certPem, requestCert: true, rejectUnauthorized: true, ca: [ca.certPem] },
        (req, res) => {
          served.push(req.url ?? "");
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
        },
      );
      mtlsServer.on("tlsClientError", () => {
        handshakeFailures++;
      });
      await new Promise<void>((resolve) => mtlsServer.listen(0, "127.0.0.1", resolve));
      const mtlsUrl = `https://127.0.0.1:${(mtlsServer.address() as AddressInfo).port}`;

      let enrollUrl = "";
      const enrollServer = http.createServer((req, res) => {
        let raw = "";
        req.on("data", (c) => (raw += c));
        req.on("end", () => {
          (async () => {
            const body = JSON.parse(raw);
            const certPem = await signCsr(opensslBin!, workDir, body.csr_pem, ca);
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end(
              JSON.stringify({
                cert_pem: certPem,
                ca_pem: ca.certPem,
                bundle_signing_pub_pem: signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString(),
                server_url: enrollUrl,
                mtls_url: mtlsUrl,
                // The whole point: this is NOT the certificate the mtls
                // listener presents.
                mtls_server_cert_pem: item.pin(serverIdentity.certPem),
              }),
            );
          })().catch((err) => {
            res.writeHead(500);
            res.end(String(err));
          });
        });
      });
      await new Promise<void>((resolve) => enrollServer.listen(0, "127.0.0.1", resolve));
      enrollUrl = `http://127.0.0.1:${(enrollServer.address() as AddressInfo).port}`;

      try {
        const enrolled = await nscp.run(["enroll", "--server", enrollUrl, "--token", "tok-pin", "--insecure"], { allowFailure: true });
        expect(enrolled.exitCode).toBe(0);
        nscp.start();

        // Give the agent several poll cycles' worth of time to get it wrong.
        await new Promise((r) => setTimeout(r, 8000));

        expect(served).toEqual([]);
        if (item.expectHandshake) {
          expect(handshakeFailures).toBeGreaterThan(0);
        } else {
          expect(handshakeFailures).toBe(0);
        }
      } finally {
        await nscp.stop();
        await new Promise<void>((resolve) => enrollServer.close(() => resolve()));
        await new Promise<void>((resolve) => mtlsServer.close(() => resolve()));
        fs.rmSync(workDir, { recursive: true, force: true });
      }
    });
  }

  it("still trusts the pinned certificate even though its subject is not the host we dialed", async () => {
    // Hostname verification is deliberately skipped for a pinned peer (the pin
    // IS the identity). This asserts the positive side of that choice: a
    // certificate whose CN/SAN say nothing about 127.0.0.1 is accepted when it
    // is the pinned one - and by the same token, the previous cases prove a
    // non-pinned certificate is not.
    const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-pinok-"));
    const nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    const ca = generateCertChain({ outDir: path.join(workDir, "tenant-ca"), signed: {} }).ca;
    const signingKeys = crypto.generateKeyPairSync("ed25519");
    const served: string[] = [];

    // No SAN for 127.0.0.1 and a CN that is not a hostname at all.
    const keys = forge.pki.rsa.generateKeyPair(2048);
    const cert = forge.pki.createCertificate();
    cert.publicKey = keys.publicKey;
    cert.serialNumber = "2002";
    cert.validity.notBefore = new Date(Date.now() - 3600_000);
    cert.validity.notAfter = new Date(Date.now() + 30 * 24 * 3600_000);
    const attrs = [{ name: "commonName", value: "fleet-mtls-endpoint-not-a-host" }];
    cert.setSubject(attrs);
    cert.setIssuer(attrs);
    cert.sign(keys.privateKey, forge.md.sha256.create());
    const serverCertPem = forge.pki.certificateToPem(cert);
    const serverKeyPem = forge.pki.privateKeyToPem(keys.privateKey);

    const mtlsServer = https.createServer(
      { key: serverKeyPem, cert: serverCertPem, requestCert: true, rejectUnauthorized: true, ca: [ca.certPem] },
      (req, res) => {
        served.push(req.url ?? "");
        const parsed = new URL(req.url ?? "/", "http://x");
        if (parsed.pathname === "/agent/v1/desired-state") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ state_hash: "pin-1", next_poll_in_seconds: 1, merged_config_json: {}, bundles: [] }));
          return;
        }
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end("{}");
      },
    );
    await new Promise<void>((resolve) => mtlsServer.listen(0, "127.0.0.1", resolve));
    const mtlsUrl = `https://127.0.0.1:${(mtlsServer.address() as AddressInfo).port}`;

    let enrollUrl = "";
    const enrollServer = http.createServer((req, res) => {
      let raw = "";
      req.on("data", (c) => (raw += c));
      req.on("end", () => {
        (async () => {
          const body = JSON.parse(raw);
          const certPem = await signCsr(opensslBin!, workDir, body.csr_pem, ca);
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(
            JSON.stringify({
              cert_pem: certPem,
              ca_pem: ca.certPem,
              bundle_signing_pub_pem: signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString(),
              server_url: enrollUrl,
              mtls_url: mtlsUrl,
              mtls_server_cert_pem: serverCertPem,
            }),
          );
        })().catch((err) => {
          res.writeHead(500);
          res.end(String(err));
        });
      });
    });
    await new Promise<void>((resolve) => enrollServer.listen(0, "127.0.0.1", resolve));
    enrollUrl = `http://127.0.0.1:${(enrollServer.address() as AddressInfo).port}`;

    try {
      expect((await nscp.run(["enroll", "--server", enrollUrl, "--token", "tok-pinok", "--insecure"], { allowFailure: true })).exitCode).toBe(0);
      nscp.start();
      const started = Date.now();
      while (!served.some((url) => url.startsWith("/agent/v1/desired-state"))) {
        if (Date.now() - started > 45_000) throw new Error("Timed out waiting for a poll over the pinned connection");
        await new Promise((r) => setTimeout(r, 150));
      }
    } finally {
      await nscp.stop();
      await new Promise<void>((resolve) => enrollServer.close(() => resolve()));
      await new Promise<void>((resolve) => mtlsServer.close(() => resolve()));
      fs.rmSync(workDir, { recursive: true, force: true });
    }
  });
});
