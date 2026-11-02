/**
 * Failure-context reports: the fleet sync sends the server a failed check,
 * with the metrics snapshot it was recorded with, once per interval.
 *
 * The contract (service/fleet_sync.cpp, onboarding::failure_context_pacer):
 *
 *  - the server opts in on its desired-state answers with
 *    `X-Failure-Context: accept`; without it nothing is sent;
 *  - at most one POST /agent/v1/failure-context per `failure context
 *    interval`, whatever fails: the failure sent is the newest one whose
 *    snapshot is still held, and every other failure since the previous report
 *    is summarised in `suppressed`;
 *  - the time of the last report is kept in applied-state.json, so a restart
 *    inside the interval sends nothing.
 *
 * Failures are produced by running CheckHelpers' check_critical / check_warning
 * over REST, so their number is exact. The fake server speaks plain http, like
 * fleet-sync.test.ts; the agent is enrolled with --insecure.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import request from "supertest";
import { NscpInstance, REST_URL, makeCertPem, FLEET_TENANT_ID } from "@fixtures/index";

jest.setTimeout(240_000);

/** The interval the agent is configured with: short, so a test sees two windows. */
const INTERVAL_SECONDS = 15;

interface SeenRequest {
  method: string;
  url: string;
  body: any;
  at: number;
}

describe("fleet failure-context reports", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;
  let requests: SeenRequest[] = [];
  let key = "";

  /** Whether desired-state answers carry `X-Failure-Context: accept`. */
  let advertise = true;
  /**
   * Polls still to answer with the full desired state rather than a 304: a
   * restarted agent only learns the 1s poll interval from a body.
   */
  let fullAnswers = 0;

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  const reports = () =>
    requests.filter((r) => r.method === "POST" && r.url === "/agent/v1/failure-context");
  const polls = () => requests.filter((r) => r.url.startsWith("/agent/v1/desired-state")).length;

  async function waitFor(
    what: string,
    predicate: () => boolean,
    timeoutMs = 60_000,
  ): Promise<void> {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  }

  /** Let the loop run `count` more poll cycles. */
  async function settle(count = 3): Promise<void> {
    const target = polls() + count;
    await waitFor(`${count} more polls`, () => polls() >= target);
  }

  async function login(): Promise<string> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    return response.body.key as string;
  }

  async function run(command: string, message: string): Promise<void> {
    await request(REST_URL)
      .get(`/api/v2/queries/${command}/commands/execute?message=${message}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
  }

  /**
   * Wait until the core has collected a metrics snapshot - the WEBServer is
   * handed the same one the core keeps - so the next failure is recorded with
   * it. Produces no failures of its own.
   */
  async function waitForSnapshot(): Promise<void> {
    const deadline = Date.now() + 30_000;
    for (;;) {
      const metrics = await request(REST_URL)
        .get("/api/v2/metrics")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true);
      if (metrics.status === 200 && Object.keys(metrics.body ?? {}).length > 0) break;
      if (Date.now() > deadline) throw new Error("no metrics snapshot was collected");
      await new Promise((r) => setTimeout(r, 500));
    }
    // The core stores its copy right after the submitters ran.
    await new Promise((r) => setTimeout(r, 1500));
  }

  /**
   * Start the agent and log in once the sync has settled. The first apply of
   * a desired state reloads the modules - the web server with them, taking
   * any session along - so the login waits for a poll the server answered
   * with 304, after which nothing reloads.
   */
  async function startAgent(): Promise<void> {
    const seen = requests.length;
    fullAnswers = 1;
    nscp.start();
    await waitFor(
      "a settled sync",
      () => requests.slice(seen).filter((r) => r.url.includes("current_hash=h-empty")).length >= 2,
    );
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    for (let attempt = 0; ; ++attempt) {
      try {
        key = await login();
        return;
      } catch (e) {
        if (attempt >= 20) throw e;
        await new Promise((r) => setTimeout(r, 500));
      }
    }
  }

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-fc-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    await nscp.configure({
      "/modules": { WEBServer: "enabled", CheckHelpers: "enabled" },
      "/settings/default": { password: "default-password", "allowed hosts": "127.0.0.1,::1" },
      "/settings/WEB/server/roles": { full: "*" },
      "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
      // A snapshot every second, kept with each failure.
      "/settings/core": { "metrics interval": "1s" },
      "/settings/core/failed checks": { "capture metrics": "true" },
      "/settings/fleet": { "failure context interval": `${INTERVAL_SECONDS}s` },
    });

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
        requests.push({ method: req.method ?? "", url: req.url ?? "", body, at: Date.now() });
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
        } else if (parsed.pathname === "/agent/v1/desired-state") {
          const headers: Record<string, string> = { "Content-Type": "application/json" };
          if (advertise) headers["X-Failure-Context"] = "accept";
          if (fullAnswers > 0) {
            --fullAnswers;
          } else if (parsed.searchParams.get("current_hash") === "h-empty") {
            res.writeHead(304, headers);
            res.end(JSON.stringify({ next_poll_in_seconds: 1 }));
            return;
          }
          res.writeHead(200, headers);
          res.end(
            JSON.stringify({
              tenant_id: FLEET_TENANT_ID,
              state_hash: "h-empty",
              next_poll_in_seconds: 1,
              merged_config_json: {},
              bundles: [],
            }),
          );
        } else if (req.method === "POST" && parsed.pathname === "/agent/v1/failure-context") {
          res.writeHead(202, { "Content-Type": "application/json" });
          res.end("{}");
        } else if (
          parsed.pathname === "/agent/v1/heartbeat" ||
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

    const enrolled = await nscp.run(
      ["enroll", "--server", baseUrl, "--token", "tok-fleet", "--insecure"],
      { allowFailure: true },
    );
    expect(enrolled.exitCode).toBe(0);
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    await startAgent();
  });

  afterAll(async () => {
    await nscp?.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  it("sends one report per interval, with the snapshot and the rest summarised", async () => {
    await waitForSnapshot();
    // Three failures while the server is not asking, so none of them goes
    // before the others are recorded; then it asks, and one report covers all
    // three.
    advertise = false;
    await settle(2);
    await run("check_critical", "one");
    await run("check_critical", "two");
    await run("check_warning", "three");
    advertise = true;
    await waitFor("the first report", () => reports().length >= 1);
    await settle(3);
    expect(reports()).toHaveLength(1);
    const first = reports()[0];
    expect(typeof first.body.sent_at).toBe("string");
    // The newest failure with a snapshot is the one sent ...
    expect(first.body.failure.command).toEqual("check_warning");
    expect(first.body.failure.message).toEqual("three");
    expect(first.body.failure.result).toEqual("WARNING");
    expect(first.body.failure.subject).toEqual("WEBServer:admin");
    expect(first.body.context.state).toEqual("captured");
    expect(first.body.context.age_seconds).toBeLessThanOrEqual(5);
    // The core's own bundle is always in the snapshot, whatever is loaded.
    expect(
      Object.keys(first.body.context.metrics).some((k: string) => k.startsWith("workers.")),
    ).toBe(true);
    expect(first.body.context.metadata).toBeDefined();
    // ... and the other two are counted, grouped by check and status.
    expect(first.body.suppressed.count).toBe(2);
    expect(first.body.suppressed.evicted).toBe(0);
    expect(first.body.suppressed.by_check).toEqual([
      expect.objectContaining({ command: "check_critical", status: 2, count: 2 }),
    ]);

    // A failure inside the window waits for it to pass.
    await run("check_critical", "four");
    await settle(3);
    expect(reports()).toHaveLength(1);
    await waitFor("the second report", () => reports().length > 1, (INTERVAL_SECONDS + 15) * 1000);
    const second = reports()[1];
    expect(second.at - first.at).toBeGreaterThanOrEqual((INTERVAL_SECONDS - 1) * 1000);
    expect(second.body.failure.message).toEqual("four");
    expect(second.body.suppressed.count).toBe(0);
    expect(second.body.suppressed.since).toEqual(first.body.sent_at);
  });

  it("keeps the last report time across a restart", async () => {
    const state = JSON.parse(
      fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"),
    );
    expect(typeof state.failure_context_sent_at).toBe("number");

    // Restarted inside the interval: a fresh failure waits for the window the
    // previous process opened rather than going at once.
    const last = reports()[reports().length - 1];
    await nscp.stop();
    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    await startAgent();
    const before = reports().length;
    await waitForSnapshot();
    await run("check_critical", "after-restart");
    const sinceLast = Date.now() - last.at;
    if (sinceLast < (INTERVAL_SECONDS - 3) * 1000) {
      await settle(2);
      expect(reports().length).toBe(before);
    }
    await waitFor(
      "a report after the restart",
      () => reports().length > before,
      (INTERVAL_SECONDS + 20) * 1000,
    );
    expect(reports()[before].at - last.at).toBeGreaterThanOrEqual((INTERVAL_SECONDS - 1) * 1000);
  });

  it("sends nothing to a server that stops asking", async () => {
    advertise = false;
    await settle(2);
    const before = reports().length;
    await run("check_critical", "unheard");
    // Past a whole interval: still nothing.
    await new Promise((r) => setTimeout(r, (INTERVAL_SECONDS + 3) * 1000));
    await settle(2);
    expect(reports().length).toBe(before);
  });
});
