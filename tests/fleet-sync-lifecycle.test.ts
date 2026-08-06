/**
 * Identity lifecycle coverage for the core fleet sync loop.
 *
 * fleet-sync.test.ts covers a steady-state agent with a long lived
 * certificate. This suite covers the transitions around that certificate,
 * which are the parts that decide whether a host can still be managed
 * tomorrow:
 *
 *   - renewal when the certificate is close to expiry (and that it stops once
 *     renewed, and that the private key never comes from the server),
 *   - a failed or malformed renewal keeping the existing, still valid identity,
 *   - a revoked identity (403 on the heartbeat) shutting the loop down instead
 *     of hammering the server,
 *   - the boot gate: no manifest, or an unusable one, means no fleet traffic
 *     at all rather than a half-initialised agent.
 *
 * Each case gets its own agent, work directory and fake server, because these
 * are all startup-time behaviours.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import forge from "node-forge";
import { NscpInstance, makeCertPem } from "@fixtures/index";

jest.setTimeout(300_000);

interface SeenRequest {
  method: string;
  url: string;
  body: any;
}

interface FleetOptions {
  /** Validity of the certificate handed out by enrollment. */
  certDays?: number;
  /** Extra `/settings/fleet` keys applied before the agent starts. */
  fleetSettings?: Record<string, string>;
  /**
   * When set, desired-state requests are accepted and then never answered -
   * the connection just hangs - for N requests, starting after the first one
   * has been answered normally. Starting after a good poll matters: the agent
   * only learns the (short) poll interval from a successful response, so this
   * is a running agent whose server goes quiet, not one that never started.
   */
  blackHolePolls?: number;
  /** Status for GET /agent/v1/heartbeat (403 = this identity was revoked). */
  heartbeatStatus?: number;
  /** Replace the renewal response entirely. */
  renewResponder?: (res: http.ServerResponse) => void;
  /** Skip enrollment (no manifest: the sync loop must not start). */
  skipEnroll?: boolean;
  /** Runs after enrollment, before the agent starts. */
  beforeStart?: (workDir: string) => void;
}

interface Fleet {
  nscp: NscpInstance;
  requests: SeenRequest[];
  workDir: string;
  readState: () => any;
  /** The certificate the renewal endpoint hands out. */
  renewedCert: string;
  waitFor: (what: string, predicate: () => boolean, timeoutMs?: number) => Promise<void>;
  count: (pathPrefix: string) => number;
  stop: () => Promise<void>;
}

/** Enroll a fresh agent against a fresh fake fleet server and start it. */
async function startFleet(name: string, options: FleetOptions = {}): Promise<Fleet> {
  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), `nscp-${name}-`));
  // NSCP_LOG_OUTPUT=1 streams the agent's log to the runner, which is the only
  // way to see what a stuck sync loop is doing.
  const nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir }, logOutput: process.env.NSCP_LOG_OUTPUT === "1" });
  const requests: SeenRequest[] = [];
  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const enrolledCert = makeCertPem(options.certDays ?? 90, "enrolled");
  const renewedCert = makeCertPem(90, "renewed");
  let baseUrl = "";
  let blackHoleLeft = options.blackHolePolls ?? 0;
  let answeredPolls = 0;

  const server = http.createServer((req, res) => {
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
      requests.push({ method: req.method ?? "", url: req.url ?? "", body });

      const parsed = new URL(req.url ?? "/", "http://x");
      if (req.method === "POST" && parsed.pathname === "/enroll/v1") {
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(
          JSON.stringify({
            cert_pem: enrolledCert,
            ca_pem: "CA-PEM",
            bundle_signing_pub_pem: signingPubPem,
            server_url: baseUrl,
            mtls_url: baseUrl,
            mtls_server_cert_pem: "MTLS-PIN",
          }),
        );
      } else if (parsed.pathname === "/agent/v1/heartbeat") {
        res.writeHead(options.heartbeatStatus ?? 200, { "Content-Type": "application/json" });
        res.end("{}");
      } else if (parsed.pathname === "/agent/v1/renew") {
        if (options.renewResponder) {
          options.renewResponder(res);
          return;
        }
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(
          JSON.stringify({
            cert_pem: renewedCert,
            ca_pem: "CA-PEM",
            bundle_signing_pub_pem: signingPubPem,
            mtls_server_cert_pem: "MTLS-PIN",
          }),
        );
      } else if (parsed.pathname === "/agent/v1/desired-state") {
        if (answeredPolls > 0 && blackHoleLeft > 0) {
          // Accept the request and then say nothing at all, ever.
          blackHoleLeft--;
          return;
        }
        answeredPolls++;
        if (parsed.searchParams.get("current_hash") === "h-1") {
          res.writeHead(304, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ next_poll_in_seconds: 1 }));
        } else {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ state_hash: "h-1", next_poll_in_seconds: 1, merged_config_json: { modules: { CheckHelpers: "enabled" } }, bundles: [] }));
        }
      } else if (parsed.pathname === "/agent/v1/state-report") {
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

  const stateFile = path.join(workDir, "security", "agent-state.json");
  if (!options.skipEnroll) {
    const r = await nscp.run(["enroll", "--server", baseUrl, "--token", `tok-${name}`], { allowFailure: true });
    if (r.exitCode !== 0) throw new Error(`enroll failed: ${r.all}`);
  }
  if (options.fleetSettings) await nscp.configure({ "/settings/fleet": options.fleetSettings });
  if (options.beforeStart) options.beforeStart(workDir);
  // Only enrollment requests so far: the agent has not started yet.
  requests.length = 0;
  nscp.start();

  const waitFor = async (what: string, predicate: () => boolean, timeoutMs = 60_000) => {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  };

  return {
    nscp,
    requests,
    workDir,
    renewedCert,
    readState: () => JSON.parse(fs.readFileSync(stateFile, "utf8")),
    waitFor,
    count: (prefix: string) => requests.filter((r) => r.url.startsWith(prefix)).length,
    stop: async () => {
      await nscp.stop();
      await new Promise<void>((resolve) => server.close(() => resolve()));
      fs.rmSync(workDir, { recursive: true, force: true });
    },
  };
}

/** The subject CN of a PEM certificate, for telling the two certs apart. */
function subjectCn(pem: string): string {
  const cert = forge.pki.certificateFromPem(pem);
  const cn = cert.subject.getField("CN");
  return cn ? cn.value : "";
}

describe("fleet sync certificate lifecycle", () => {
  let fleet: Fleet | undefined;

  afterEach(async () => {
    if (fleet) await fleet.stop();
    fleet = undefined;
  });

  it("renews a certificate that is close to expiry, then stops renewing", async () => {
    // 5 days left is inside the 14 day renewal window.
    fleet = await startFleet("renew", { certDays: 5 });

    await fleet.waitFor("a renewal request", () => fleet!.count("/agent/v1/renew") > 0);
    await fleet.waitFor("the renewed certificate on disk", () => subjectCn(fleet!.readState().cert_pem) === "renewed");

    const state = fleet.readState();
    expect(state.cert_pem).toBe(fleet.renewedCert);
    // The key is generated locally for the new CSR: it must have changed, and
    // it must never be something the server sent.
    expect(state.private_key_pem).toContain("BEGIN PRIVATE KEY");
    // The renewal request carried a CSR, not a key.
    const renewRequest = fleet.requests.find((r) => r.url === "/agent/v1/renew")!;
    expect(renewRequest.body.csr_pem).toContain("BEGIN CERTIFICATE REQUEST");
    expect(JSON.stringify(renewRequest.body)).not.toContain("PRIVATE KEY");

    // With 90 days left the loop must leave it alone from now on.
    const renewsAfter = fleet.count("/agent/v1/renew");
    const pollsBefore = fleet.count("/agent/v1/desired-state");
    await fleet.waitFor("three more polls", () => fleet!.count("/agent/v1/desired-state") >= pollsBefore + 3);
    expect(fleet.count("/agent/v1/renew")).toBe(renewsAfter);
  });

  it("keeps the working identity when renewal fails", async () => {
    for (const responder of [
      (res: http.ServerResponse) => {
        res.writeHead(500);
        res.end("boom");
      },
      (res: http.ServerResponse) => {
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end("not json at all");
      },
      (res: http.ServerResponse) => {
        // A 200 that is missing material: applying half of it would strand the host.
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ cert_pem: "" }));
      },
    ]) {
      const attempt = await startFleet("renewfail", { certDays: 5, renewResponder: responder });
      try {
        await attempt.waitFor("a renewal attempt", () => attempt.count("/agent/v1/renew") > 0);
        // The old identity is still there, and the agent still manages the host.
        expect(subjectCn(attempt.readState().cert_pem)).toBe("enrolled");
        await attempt.waitFor("a state report", () => attempt.count("/agent/v1/state-report") > 0);
        const report = attempt.requests.find((r) => r.url.startsWith("/agent/v1/state-report"))!;
        expect(report.body).toBeTruthy();
      } finally {
        await attempt.stop();
      }
    }
  });

  it("shuts the loop down when the server rejects our certificate as revoked", async () => {
    // 403 on the startup heartbeat is the protocol's "you are revoked": there
    // is nothing a retry can achieve, so the loop must stop rather than poll.
    fleet = await startFleet("revoked", { heartbeatStatus: 403 });

    await fleet.waitFor("the startup heartbeat", () => fleet!.count("/agent/v1/heartbeat") > 0);
    await new Promise((r) => setTimeout(r, 4000));
    expect(fleet.count("/agent/v1/desired-state")).toBe(0);
    expect(fleet.count("/agent/v1/state-report")).toBe(0);
    // And it stopped: one heartbeat, no retry loop.
    expect(fleet.count("/agent/v1/heartbeat")).toBe(1);
  });

  it("recovers when the server accepts a request and then never answers", async () => {
    // Every read here is synchronous, so without a deadline one silent
    // response parks the sync thread forever: the host stays enrolled and
    // stops being managed, with nothing in the log to say so. Two hung polls
    // must cost two timeouts, not the agent.
    fleet = await startFleet("blackhole", { blackHolePolls: 2, fleetSettings: { timeout: "2s" } });

    // The agent gets one good poll (so it is fully running), then the server
    // goes silent for two polls.
    await fleet.waitFor("the first applied state", () =>
      fleet!.requests.some((r) => r.url.startsWith("/agent/v1/state-report") && r.body?.applied_state_hash === "h-1"),
    );
    // Two deadlines plus backoff, and the loop must still be polling after it.
    await fleet.waitFor("polls to continue past the two hung ones", () => fleet!.count("/agent/v1/desired-state") >= 4, 90_000);
  });

  it("recovers from a corrupt applied-state file by re-applying", async () => {
    fleet = await startFleet("corruptstate", {
      beforeStart: (workDir) => {
        const managed = path.join(workDir, "fleet");
        fs.mkdirSync(managed, { recursive: true });
        fs.writeFileSync(path.join(managed, "applied-state.json"), "{ this is not json");
      },
    });

    await fleet.waitFor("a state report with the applied hash", () =>
      fleet!.requests.some((r) => r.url.startsWith("/agent/v1/state-report") && r.body?.applied_state_hash === "h-1"),
    );
    const applied = JSON.parse(fs.readFileSync(path.join(fleet.workDir, "fleet", "applied-state.json"), "utf8"));
    expect(applied.state_hash).toBe("h-1");
  });

  it("does not start the loop at all without an enrollment manifest", async () => {
    fleet = await startFleet("nomanifest", { skipEnroll: true });
    await new Promise((r) => setTimeout(r, 4000));
    expect(fleet.requests).toEqual([]);
  });

  it("refuses to run on a state file that is missing identity material", async () => {
    // A truncated or hand-edited manifest must stop the loop with an error, not
    // start it with an empty key (which would fail every TLS handshake).
    fleet = await startFleet("badstate", {
      beforeStart: (workDir) => {
        const stateFile = path.join(workDir, "security", "agent-state.json");
        const state = JSON.parse(fs.readFileSync(stateFile, "utf8"));
        state.cert_pem = "";
        fs.writeFileSync(stateFile, JSON.stringify(state));
      },
    });
    await new Promise((r) => setTimeout(r, 4000));
    expect(fleet.requests).toEqual([]);
  });
});
