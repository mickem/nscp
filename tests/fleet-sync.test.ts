/**
 * End-to-end test of the core fleet sync loop against a fake fleet server.
 *
 * The sync loop lives in the service core (service/fleet_sync.cpp), not in a
 * module: `nscp enroll` writes the enrollment manifest (agent-state.json) and
 * the fleet.ini include, and the core starts the sync thread at boot purely
 * because the manifest exists. Flow under test: heartbeat, desired-state
 * poll, bundle download, SHA-256 + Ed25519 verification, unzip, JSON merge
 * patch, INI render, atomic swap, state report, metrics, then 304
 * steady-state. A second phase serves a tampered bundle and asserts it is
 * rejected and reported without touching the applied configuration.
 *
 * The fake server speaks plain http; the agent treats mtls_url's scheme as
 * authoritative, which keeps the test transport-simple while production uses
 * https with a pinned certificate.
 *
 * Bundle zips are hand-built (store method, real CRC32) to avoid a zip
 * dependency; signing uses node's built-in Ed25519.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import forge from "node-forge";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

// --- minimal stored-zip writer ----------------------------------------------

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c;
  }
  return t;
})();

function crc32(buf: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function makeZip(entries: { name: string; data: string | Buffer }[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "utf8");
    const data = Buffer.isBuffer(e.data) ? e.data : Buffer.from(e.data, "utf8");
    const crc = crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4); // version needed
    local.writeUInt16LE(0x21, 12); // dos date 1980-01-01
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(name.length, 26);
    locals.push(local, name, data);
    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(0x21, 14);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt32LE(offset, 42);
    centrals.push(central, name);
    offset += 30 + name.length + data.length;
  }
  const cd = Buffer.concat(centrals);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0);
  eocd.writeUInt16LE(entries.length, 8);
  eocd.writeUInt16LE(entries.length, 10);
  eocd.writeUInt32LE(cd.length, 12);
  eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, cd, eocd]);
}

// --- fleet crypto -------------------------------------------------------------

/** Ed25519 signature over the 32-byte SHA-256 digest, base64 - the fleet wire format. */
function signBundle(privateKey: crypto.KeyObject, bytes: Buffer): string {
  const digest = crypto.createHash("sha256").update(bytes).digest();
  return crypto.sign(null, digest, privateKey).toString("base64");
}

/** A parseable (RSA, self-signed) certificate valid for `days`, for cert_pem. */
function makeCertPem(days: number): string {
  const keys = forge.pki.rsa.generateKeyPair(2048);
  const cert = forge.pki.createCertificate();
  cert.publicKey = keys.publicKey;
  cert.serialNumber = "01";
  cert.validity.notBefore = new Date(Date.now() - 3600_000);
  cert.validity.notAfter = new Date(Date.now() + days * 24 * 3600_000);
  const attrs = [{ name: "commonName", value: "fleet-agent" }];
  cert.setSubject(attrs);
  cert.setIssuer(attrs);
  cert.sign(keys.privateKey);
  return forge.pki.certificateToPem(cert);
}

// --- the suite ----------------------------------------------------------------

interface SeenRequest {
  method: string;
  url: string;
  body: any;
}

describe("core fleet sync loop", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;
  let requests: SeenRequest[];

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  // Good bundle: config + a script file.
  const goodZip = makeZip([
    { name: "bundle.toml", data: 'name = "demo"\nversion = "1.0"\n' },
    {
      name: "config.json",
      data: JSON.stringify({
        modules: { CheckHelpers: "enabled" },
        settings: { "fleet demo": { greeting: "hello", retries: 3 } },
      }),
    },
    { name: "scripts/demo/hello.txt", data: "hello fleet\n" },
  ]);
  const goodSha = crypto.createHash("sha256").update(goodZip).digest("hex");

  // Tampered bundle: valid digest but signed by the WRONG key.
  const evilZip = makeZip([{ name: "config.json", data: JSON.stringify({ modules: { EvilModule: "enabled" } }) }]);
  const evilSha = crypto.createHash("sha256").update(evilZip).digest("hex");
  const wrongKeys = crypto.generateKeyPairSync("ed25519");

  // A later revision of the good bundle that drops hello.txt and adds another
  // script: what the agent must do with scripts that left the desired state.
  const trimmedZip = makeZip([
    { name: "bundle.toml", data: 'name = "demo"\nversion = "2.0"\n' },
    { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" } }) },
    { name: "scripts/demo/other.txt", data: "other fleet\n" },
  ]);
  const trimmedSha = crypto.createHash("sha256").update(trimmedZip).digest("hex");

  /** Mutable server behavior: which desired state is currently served. */
  let phase: "good" | "evil" | "trimmed";

  function desiredStateFor(currentHash: string | null): { code: number; body: any } {
    const states = {
      good: {
        state_hash: "h-good",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-good",
            name: "demo",
            version: "1.0",
            sha256: goodSha,
            signature: signBundle(signingKeys.privateKey, goodZip),
            url: "/agent/v1/bundles/b-good",
            priority: 100,
          },
        ],
      },
      evil: {
        state_hash: "h-evil",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-evil",
            name: "evil",
            version: "6.6.6",
            sha256: evilSha,
            signature: signBundle(wrongKeys.privateKey, evilZip),
            url: "/agent/v1/bundles/b-evil",
            priority: 100,
          },
        ],
      },
      trimmed: {
        state_hash: "h-trimmed",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-trimmed",
            name: "demo",
            version: "2.0",
            sha256: trimmedSha,
            signature: signBundle(signingKeys.privateKey, trimmedZip),
            url: "/agent/v1/bundles/b-trimmed",
            priority: 100,
          },
        ],
      },
    };
    const active = states[phase];
    if (currentHash === active.state_hash) {
      return { code: 304, body: { next_poll_in_seconds: 1 } };
    }
    return { code: 200, body: active };
  }

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    requests = [];
    phase = "good";

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
        } else if (parsed.pathname === "/agent/v1/bundles/b-good") {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(goodZip);
        } else if (parsed.pathname === "/agent/v1/bundles/b-evil") {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(evilZip);
        } else if (parsed.pathname === "/agent/v1/bundles/b-trimmed") {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(trimmedZip);
        } else if (parsed.pathname === "/agent/v1/state-report" || parsed.pathname === "/agent/v1/renew") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
        } else if (parsed.pathname === "/agent/v1/metrics") {
          res.writeHead(204);
          res.end();
        } else {
          res.writeHead(404);
          res.end("not found");
        }
      });
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;

    // Fetch metrics from the modules every second: the sync loop submits the
    // latest snapshot, so the default 10s would make the metrics assertion
    // wait for the first fetch.
    await nscp.configure({ "/settings/core": { "metrics interval": "1s" } });
  });

  afterAll(async () => {
    await nscp.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  async function waitFor(what: string, predicate: () => boolean, timeoutMs = 45_000): Promise<void> {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  }

  const stateReports = () => requests.filter((r) => r.url.startsWith("/agent/v1/state-report"));

  it("enrolls, writing the manifest and the fleet.ini include (no module needed)", async () => {
    const r = await nscp.run(["enroll", "--server", baseUrl, "--token", "tok-fleet"], { allowFailure: true });
    expect(r.exitCode).toBe(0);
    // The manifest is the activation switch: its presence is what makes the
    // core start the sync thread at boot.
    expect(fs.existsSync(path.join(workDir, "security", "agent-state.json"))).toBe(true);

    const ini = fs.readFileSync(nscp.settingsFile, "utf8");
    // The sync loop lives in the core; enrollment must NOT enable any module.
    expect(ini).not.toContain("NSClientConfig");
    expect(ini).toMatch(/\[\/includes\]/);
    expect(ini).toMatch(/fleet\s*=.*fleet[/\\]fleet\.ini/);
    // The include target exists as a placeholder so boot never logs a
    // missing-file error before the first sync.
    expect(fs.existsSync(path.join(workDir, "fleet", "fleet.ini"))).toBe(true);
  });

  it("syncs desired state: verifies, applies and reports", async () => {
    nscp.start();

    // The loop applies the good state and reports the hash.
    await waitFor("state report with applied hash", () => stateReports().some((r) => r.body?.applied_state_hash === "h-good"));

    const report = stateReports().find((r) => r.body?.applied_state_hash === "h-good")!;
    expect(report.body.bundles_installed).toEqual([{ id: "b-good", version: "1.0" }]);
    expect(report.body.errors).toEqual([]);
    expect(report.body.reported_tags.os).toBeTruthy();

    // Rendered INI: merged JSON -> sections, deterministic.
    const fleetIni = fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
    expect(fleetIni).toContain("[/modules]");
    expect(fleetIni).toContain("CheckHelpers=enabled");
    expect(fleetIni).toContain("[/settings/fleet demo]");
    expect(fleetIni).toContain("greeting=hello");
    expect(fleetIni).toContain("retries=3");

    // Scripts staged under the managed path; bundle cached by id+sha.
    expect(fs.readFileSync(path.join(workDir, "fleet", "scripts", "demo", "hello.txt"), "utf8")).toContain("hello fleet");
    expect(fs.existsSync(path.join(workDir, "fleet", "cache", `b-good-${goodSha.substring(0, 16)}.zip`))).toBe(true);

    // Applied hash persisted for restarts.
    const applied = JSON.parse(fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"));
    expect(applied.state_hash).toBe("h-good");

    // Startup calls happened: heartbeat + an early tag report before the poll.
    expect(requests.some((r) => r.url === "/agent/v1/heartbeat")).toBe(true);
    const firstDesired = requests.findIndex((r) => r.url.startsWith("/agent/v1/desired-state"));
    const firstReport = requests.findIndex((r) => r.url.startsWith("/agent/v1/state-report"));
    expect(firstReport).toBeGreaterThanOrEqual(0);
    expect(firstReport).toBeLessThan(firstDesired);
  });

  it("settles into 304 polling with the applied hash and submits the core metrics set", async () => {
    await waitFor("a 304 steady-state poll", () => requests.some((r) => r.url === "/agent/v1/desired-state?current_hash=h-good"));

    // What the agent submits is the core's own metrics snapshot (the same set
    // the REST /api/v2/metrics view serves), not a synthetic sample: the core
    // always publishes a "workers" bundle, so that one is always there.
    const withWorkers = () =>
      requests.filter((r) => r.url === "/agent/v1/metrics" && (r.body?.samples ?? []).some((s: any) => String(s.key).startsWith("workers.")));
    await waitFor("a metrics submission carrying the core metrics set", () => withWorkers().length > 0);

    const samples = withWorkers().pop()!.body.samples as { key: string; value: number; ts?: number }[];
    // Agent uptime is reported alongside them, and is ours (no module has it).
    expect(samples.some((s) => s.key === "uptime_seconds")).toBe(true);

    const worker = samples.find((s) => s.key.startsWith("workers."))!;
    expect(typeof worker.value).toBe("number");
    // Snapshots are timestamped where they are collected: they wait for the
    // next poll, which can be a whole poll interval later.
    expect(typeof worker.ts).toBe("number");
    expect(worker.ts).toBeGreaterThan(1_700_000_000);
  });

  it("rejects a bundle signed with the wrong key and keeps the old config", async () => {
    const reportsBefore = stateReports().length;
    phase = "evil";

    // A failed apply is reported WITHOUT applied_state_hash and with errors.
    await waitFor("a failure report", () =>
      stateReports()
        .slice(reportsBefore)
        .some((r) => Array.isArray(r.body?.errors) && r.body.errors.length > 0),
    );
    const failure = stateReports()
      .slice(reportsBefore)
      .find((r) => r.body?.errors?.length > 0)!;
    expect(failure.body.applied_state_hash).toBeUndefined();
    expect(String(failure.body.errors[0])).toMatch(/signature|verification/i);

    // The previous configuration stays applied: never apply an unverified bundle.
    const fleetIni = fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
    expect(fleetIni).toContain("CheckHelpers=enabled");
    expect(fleetIni).not.toContain("EvilModule");
    const applied = JSON.parse(fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"));
    expect(applied.state_hash).toBe("h-good");
    // The unverified bundle must never enter the cache.
    expect(fs.existsSync(path.join(workDir, "fleet", "cache", `b-evil-${evilSha.substring(0, 16)}.zip`))).toBe(false);
  });

  it("drops scripts that left the desired state", async () => {
    const scripts = path.join(workDir, "fleet", "scripts", "demo");
    expect(fs.existsSync(path.join(scripts, "hello.txt"))).toBe(true);

    phase = "trimmed";
    await waitFor("state report with the trimmed hash", () => stateReports().some((r) => r.body?.applied_state_hash === "h-trimmed"));

    // The new bundle's script is there and the one it replaced is gone: a
    // script removed from a bundle must not linger and keep running.
    expect(fs.readFileSync(path.join(scripts, "other.txt"), "utf8")).toContain("other fleet");
    expect(fs.existsSync(path.join(scripts, "hello.txt"))).toBe(false);
  });
});
