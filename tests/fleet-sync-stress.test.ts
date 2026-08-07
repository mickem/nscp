/**
 * Reload/apply stress for the fleet sync loop.
 *
 * Every applied configuration makes the fleet thread call reload(), which adds
 * a task to the core scheduler from a thread that is not the scheduler's own,
 * while the scheduler worker is dispatching tasks (metrics run every second
 * here). That is a narrow window that ordinary tests hit once; this suite hits
 * it continuously by serving a new state hash on every poll.
 *
 * What it asserts is simply that the agent keeps working: it must keep
 * applying, keep reporting, and still be alive at the end. A crash, a wedged
 * scheduler or a dead sync thread all show up as "the reports stopped".
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import { NscpInstance, makeZip, bundleEntry, makeCertPem } from "@fixtures/index";

jest.setTimeout(300_000);

/** How long to hammer apply+reload. */
const STRESS_MS = Number(process.env.NSCP_FLEET_STRESS_MS ?? 60_000);

describe("fleet sync under continuous apply/reload", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  /** Distinct bundles so every poll really is a new desired state. */
  const bundles = Array.from({ length: 8 }, (_, i) =>
    makeZip([
      { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" }, settings: { stress: { round: String(i) } } }) },
      { name: `scripts/demo/round-${i}.txt`, data: `round ${i}\n` },
    ]),
  );

  let round = 0;
  const appliedHashes: string[] = [];
  let polls = 0;

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-stress-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });

    server = http.createServer((req, res) => {
      const chunks: Buffer[] = [];
      req.on("data", (c: Buffer) => chunks.push(c));
      req.on("end", () => {
        const raw = Buffer.concat(chunks).toString();
        let body: any = raw;
        try {
          body = JSON.parse(raw);
        } catch {
          /* keep raw */
        }
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
          // A new state every single poll: apply -> reload, forever.
          polls++;
          const index = round++ % bundles.length;
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(
            JSON.stringify({
              state_hash: `h-${round}`,
              next_poll_in_seconds: 1,
              merged_config_json: {},
              bundles: [bundleEntry(`b-${index}`, bundles[index], signingKeys.privateKey)],
            }),
          );
        } else if (parsed.pathname.startsWith("/agent/v1/bundles/")) {
          const index = Number(parsed.pathname.substring("/agent/v1/bundles/b-".length));
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(bundles[index]);
        } else if (parsed.pathname === "/agent/v1/state-report") {
          if (body?.applied_state_hash) appliedHashes.push(body.applied_state_hash);
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

    expect((await nscp.run(["enroll", "--server", baseUrl, "--token", "tok-stress", "--insecure"], { allowFailure: true })).exitCode).toBe(0);
    // Metrics every second: the scheduler worker is then busy in the same
    // structures the reload task is being added to.
    await nscp.configure({ "/settings/core": { "metrics interval": "1s" } });
  });

  afterAll(async () => {
    await nscp.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
    fs.rmSync(workDir, { recursive: true, force: true });
  });

  it("keeps applying and reporting while reloading continuously", async () => {
    nscp.start();

    const started = Date.now();
    let lastCount = 0;
    let stalledFor = 0;
    while (Date.now() - started < STRESS_MS) {
      await new Promise((r) => setTimeout(r, 1000));
      if (appliedHashes.length === lastCount) {
        stalledFor += 1000;
      } else {
        stalledFor = 0;
        lastCount = appliedHashes.length;
      }
      // 20s without a single applied state means the agent stopped: a crash, a
      // wedged scheduler or a dead sync thread.
      expect(stalledFor).toBeLessThan(20_000);
    }

    // Sanity: with a 1s poll interval this should be dozens of applies.
    expect(appliedHashes.length).toBeGreaterThan(STRESS_MS / 5_000);
    expect(polls).toBeGreaterThan(STRESS_MS / 5_000);
    // And the configuration is whatever the last round asked for.
    const ini = fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
    expect(ini).toContain("[/settings/stress]");
  });
});
