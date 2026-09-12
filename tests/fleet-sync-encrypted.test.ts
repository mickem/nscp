/**
 * Encrypted fleet bundles ("enc-v1") end to end.
 *
 * The operator seals a bundle in the browser before uploading it, so the fleet
 * server only ever stores and serves an NSEB1 envelope:
 *
 *   "NSEB1" | key fingerprint (8) | nonce (12) | AES-256-GCM ciphertext | tag (16)
 *
 * with `name || 0x00 || version` as the additional data. The key reaches the
 * agent out of band (`nscp enroll --bundle-key`), never from the server. The
 * server's SHA-256 and Ed25519 signature cover the envelope, so the download
 * checks are unchanged and decryption is a step after them.
 *
 * Under test: a sealed bundle is opened and applied when the host holds the
 * key; the plaintext never lands in the cache (only the envelope does); a
 * bundle served under another name is refused, as is one sealed with a key
 * this host does not hold; `require encrypted bundles` refuses a plain one.
 * The envelope is built with node's own AES-GCM, an implementation
 * independent of the agent's.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import { NscpInstance, makeZip, signBundle, makeCertPem } from "@fixtures/index";

jest.setTimeout(180_000);

interface SeenRequest {
  method: string;
  url: string;
  body: any;
}

/** Seal `plaintext` exactly the way the fleet server's browser code does. */
function seal(key: Buffer, name: string, version: string, plaintext: Buffer): Buffer {
  const nonce = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv("aes-256-gcm", key, nonce);
  cipher.setAAD(
    Buffer.concat([Buffer.from(name, "utf8"), Buffer.from([0]), Buffer.from(version, "utf8")]),
  );
  const ct = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  const fingerprint = crypto.createHash("sha256").update(key).digest().subarray(0, 8);
  return Buffer.concat([
    Buffer.from("NSEB1", "ascii"),
    fingerprint,
    nonce,
    ct,
    cipher.getAuthTag(),
  ]);
}

describe("encrypted fleet bundles", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;
  let requests: SeenRequest[];

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  const bundleKey = crypto.randomBytes(32);
  const otherKey = crypto.randomBytes(32);
  const fingerprintOf = (key: Buffer) =>
    crypto.createHash("sha256").update(key).digest().subarray(0, 8).toString("hex");

  const secretZip = makeZip([
    { name: "bundle.toml", data: 'name = "sealed"\nversion = "1.0"\n' },
    {
      name: "config.json",
      data: JSON.stringify({
        modules: { CheckHelpers: "enabled" },
        settings: { "fleet sealed": { "api token": "hunter2-sealed-value" } },
      }),
    },
    { name: "scripts/sealed/secret.txt", data: "sealed script body\n" },
  ]);
  const sealedGood = seal(bundleKey, "sealed", "1.0", secretZip);
  const sealedOther = seal(otherKey, "foreign", "1.0", secretZip);
  const plainZip = makeZip([
    { name: "bundle.toml", data: 'name = "plain"\nversion = "1.0"\n' },
    { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" } }) },
  ]);

  const sha = (b: Buffer) => crypto.createHash("sha256").update(b).digest("hex");

  let phase: "sealed" | "relabelled" | "foreign" | "plain" | "plain-required";

  function desiredStateFor(currentHash: string | null): { code: number; body: any } {
    const entry = (id: string, name: string, version: string, bytes: Buffer, format: string) => ({
      id,
      name,
      version,
      sha256: sha(bytes),
      signature: signBundle(signingKeys.privateKey, bytes),
      url: `/agent/v1/bundles/${id}`,
      priority: 100,
      format,
    });
    const states = {
      sealed: {
        state_hash: "h-sealed",
        bundles: [entry("b-sealed", "sealed", "1.0", sealedGood, "enc-v1")],
      },
      // Same envelope, same signature, but the server claims another version:
      // the additional data no longer matches and the seal must not open.
      relabelled: {
        state_hash: "h-relabelled",
        bundles: [entry("b-relabelled", "sealed", "2.0", sealedGood, "enc-v1")],
      },
      foreign: {
        state_hash: "h-foreign",
        bundles: [entry("b-foreign", "foreign", "1.0", sealedOther, "enc-v1")],
      },
      plain: {
        state_hash: "h-plain",
        bundles: [entry("b-plain", "plain", "1.0", plainZip, "plain")],
      },
      // The same plain bundle under a new hash, so a restarted agent that has
      // already applied h-plain gets it offered again rather than a 304.
      "plain-required": {
        state_hash: "h-plain-required",
        bundles: [entry("b-plain", "plain", "1.0", plainZip, "plain")],
      },
    };
    const active = { next_poll_in_seconds: 1, merged_config_json: {}, ...states[phase] };
    if (currentHash === active.state_hash) return { code: 304, body: { next_poll_in_seconds: 1 } };
    return { code: 200, body: active };
  }

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-enc-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    requests = [];
    phase = "sealed";
    server = http.createServer((req, res) => {
      let raw = "";
      req.on("data", (chunk) => (raw += chunk));
      req.on("end", () => {
        let body: any = raw;
        try {
          body = JSON.parse(raw);
        } catch {
          /* keep raw */
        }
        requests.push({ method: req.method ?? "", url: req.url ?? "", body });
        const parsed = new URL(req.url ?? "/", "http://x");
        const bundles: Record<string, Buffer> = {
          "/agent/v1/bundles/b-sealed": sealedGood,
          "/agent/v1/bundles/b-relabelled": sealedGood,
          "/agent/v1/bundles/b-foreign": sealedOther,
          "/agent/v1/bundles/b-plain": plainZip,
        };
        if (req.method === "POST" && parsed.pathname === "/enroll/v1") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(
            JSON.stringify({
              cert_pem: agentCertPem,
              ca_pem: "CA-PEM",
              bundle_signing_pub_pem: signingPubPem,
              server_url: baseUrl,
              mtls_url: baseUrl,
              mtls_server_cert_pem: "MTLS-PIN",
            }),
          );
        } else if (parsed.pathname === "/agent/v1/heartbeat") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
        } else if (parsed.pathname === "/agent/v1/desired-state") {
          const r = desiredStateFor(parsed.searchParams.get("current_hash"));
          res.writeHead(r.code, { "Content-Type": "application/json" });
          res.end(JSON.stringify(r.body));
        } else if (bundles[parsed.pathname]) {
          const bytes = bundles[parsed.pathname];
          // The real server picks the content type by sniffing the magic.
          res.writeHead(200, {
            "Content-Type":
              bytes.subarray(0, 5).toString("ascii") === "NSEB1"
                ? "application/octet-stream"
                : "application/zip",
          });
          res.end(bytes);
        } else if (
          parsed.pathname === "/agent/v1/state-report" ||
          parsed.pathname === "/agent/v1/renew"
        ) {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
        } else {
          res.writeHead(404);
          res.end("not found");
        }
      });
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;
  });

  afterAll(async () => {
    await nscp.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  async function waitFor(
    what: string,
    predicate: () => boolean,
    timeoutMs = 45_000,
  ): Promise<void> {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  }
  const stateReports = () => requests.filter((r) => r.url.startsWith("/agent/v1/state-report"));
  const reportWithError = (needle: string) =>
    stateReports().find(
      (r) => Array.isArray(r.body?.errors) && r.body.errors.some((e: string) => e.includes(needle)),
    );

  it("stores the bundle key given at enrollment in the manifest, not the settings", async () => {
    const r = await nscp.run(
      [
        "enroll",
        "--server",
        baseUrl,
        "--token",
        "tok-fleet",
        "--insecure",
        "--bundle-key",
        bundleKey.toString("base64"),
      ],
      { allowFailure: true },
    );
    expect(r.exitCode).toBe(0);
    expect(r.all ?? r.stdout).toContain(`fingerprint ${fingerprintOf(bundleKey)}`);
    const state = JSON.parse(
      fs.readFileSync(path.join(workDir, "security", "agent-state.json"), "utf8"),
    );
    expect(state.bundle_keys).toEqual([bundleKey.toString("base64")]);
    expect(fs.readFileSync(nscp.settingsFile, "utf8")).not.toContain(bundleKey.toString("base64"));
  });

  it("opens a sealed bundle and applies it, caching only the envelope", async () => {
    nscp.start();
    await waitFor("state report with the sealed hash", () =>
      stateReports().some((r) => r.body?.applied_state_hash === "h-sealed"),
    );
    const report = stateReports().find((r) => r.body?.applied_state_hash === "h-sealed")!;
    expect(report.body.errors).toEqual([]);
    expect(report.body.bundles_installed).toEqual([{ id: "b-sealed", version: "1.0" }]);

    const fleetIni = fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
    expect(fleetIni).toContain("CheckHelpers=enabled");
    expect(fleetIni).toContain("api token=hunter2-sealed-value");
    expect(
      fs.readFileSync(path.join(workDir, "fleet", "scripts", "sealed", "secret.txt"), "utf8"),
    ).toContain("sealed script body");

    // The cache holds what the signature covers: the envelope, not the zip.
    const cached = fs.readFileSync(
      path.join(workDir, "fleet", "cache", `b-sealed-${sha(sealedGood).substring(0, 16)}.zip`),
    );
    expect(cached.equals(sealedGood)).toBe(true);
    expect(cached.includes("hunter2-sealed-value")).toBe(false);
    // And the unsealed copy used for extraction is gone again.
    const staging = path.join(workDir, "fleet", "staging");
    if (fs.existsSync(staging)) {
      expect(fs.readdirSync(staging).filter((f) => f.endsWith(".unsealed.zip"))).toEqual([]);
    }
  });

  it("refuses the same envelope served under another version", async () => {
    phase = "relabelled";
    await waitFor(
      "a report rejecting the relabelled bundle",
      () => reportWithError("b-relabelled") !== undefined,
    );
    const report = reportWithError("b-relabelled")!;
    // A failed apply reports no applied hash at all (the field is omitted).
    expect(report.body.applied_state_hash ?? null).toBeNull();
    expect(report.body.errors.join(" ")).toMatch(/authentication failed/);
    // The previously applied configuration is untouched.
    expect(fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8")).toContain(
      "api token=hunter2-sealed-value",
    );
  });

  it("names the missing key's fingerprint when it holds no matching key", async () => {
    phase = "foreign";
    await waitFor(
      "a report rejecting the foreign bundle",
      () => reportWithError("b-foreign") !== undefined,
    );
    const errors = reportWithError("b-foreign")!.body.errors.join(" ");
    expect(errors).toContain(fingerprintOf(otherKey));
    expect(errors).not.toContain(bundleKey.toString("base64"));
    expect(errors).not.toContain(otherKey.toString("base64"));
  });

  it("still applies plain bundles unless encrypted ones are required", async () => {
    phase = "plain";
    await waitFor("the plain bundle applied", () =>
      stateReports().some((r) => r.body?.applied_state_hash === "h-plain"),
    );

    await nscp.stop();
    await nscp.configure({ "/settings/fleet": { "require encrypted bundles": "true" } });
    requests = [];
    phase = "plain-required";
    nscp.start();
    await waitFor(
      "a report refusing the plain bundle",
      () => reportWithError("b-plain") !== undefined,
    );
    expect(reportWithError("b-plain")!.body.errors.join(" ")).toMatch(/requires encrypted bundles/);
    expect(stateReports().some((r) => r.body?.applied_state_hash === "h-plain-required")).toBe(
      false,
    );
  });
});
