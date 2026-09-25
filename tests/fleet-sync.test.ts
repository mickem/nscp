/**
 * End-to-end test of the core fleet sync loop against a fake fleet server.
 *
 * The sync loop lives in the service core (service/fleet_sync.cpp), not in a
 * module: `nscp enroll` writes the enrollment manifest (agent-state.json) and
 * the fleet.ini include, and the core starts the sync thread at boot purely
 * because the manifest exists. Flow under test: heartbeat, desired-state
 * poll, bundle download, SHA-256 + Ed25519 verification, unzip, JSON merge
 * patch, INI render, atomic swap, state report, then 304
 * steady-state. A second phase serves a tampered bundle and asserts it is
 * rejected and reported without touching the applied configuration. The last
 * phases cover the host facts: the hash in every state report, and the
 * document on /agent/v1/facts once a bundle enables a fact set, again when the
 * server says it holds something else, and not at all against a server that
 * predates the call.
 *
 * The fake server speaks plain http; the agent treats mtls_url's scheme as
 * authoritative, which keeps the test transport-simple while production uses
 * https with a pinned certificate.
 *
 * Bundle zips are hand-built (store method, real CRC32) to avoid a zip
 * dependency; signing uses node's built-in Ed25519. Both live in
 * @fixtures/fleet, shared with the hostile and mTLS suites.
 *
 * See also fleet-sync-hostile.test.ts (what a compromised server can send),
 * fleet-sync-lifecycle.test.ts (renewal, revocation, boot gate) and
 * fleet-sync-mtls.test.ts (real mTLS and certificate pinning).
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import {
  NscpInstance,
  makeZip,
  signBundle,
  makeCertPem,
  FLEET_TENANT_ID,
  onWindows,
  moduleBuiltHere,
} from "@fixtures/index";

jest.setTimeout(180_000);

// --- the suite ----------------------------------------------------------------

interface SeenRequest {
  method: string;
  url: string;
  body: any;
  raw: string;
}

/** The module the agent is configured with locally; see the beforeAll. */
const LOCAL_MODULE = moduleBuiltHere("CheckDisk") ? "CheckDisk" : "CheckHelpers";

/** sha256("{}"): the facts hash of a host with nothing enabled. */
const EMPTY_FACTS_HASH = "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";

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
  const evilZip = makeZip([
    { name: "config.json", data: JSON.stringify({ modules: { EvilModule: "enabled" } }) },
  ]);
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

  // Fleet-managed enablement of a fact set: the bundle loads the module that
  // produces `os` and flips its switch, which is plain INI like any other
  // setting.
  const factsZip = makeZip([
    { name: "bundle.toml", data: 'name = "inventory"\nversion = "1.0"\n' },
    {
      name: "config.json",
      data: JSON.stringify({
        modules: { CheckSystem: "enabled" },
        settings: { system: { [onWindows ? "windows" : "unix"]: { facts: { os: true } } } },
      }),
    },
  ]);
  const factsSha = crypto.createHash("sha256").update(factsZip).digest("hex");

  /** Mutable server behavior: which desired state is currently served. */
  let phase: "good" | "evil" | "trimmed" | "gone" | "facts";
  /** What the fake server does with a facts upload. */
  let factsStatus = 200;
  /** The facts hash the fake server holds for the host (set by an upload). */
  let heldFactsHash = "";
  /** Whether the fake server talks about facts at all (carries facts_hash). */
  let advertiseFacts = false;
  /**
   * Answer every poll with the full desired state, never 304. A 304 carries
   * no body - node strips it, as HTTP says - so this is how a server gets a
   * facts_hash (or a poll interval) to an agent that is already in sync.
   */
  let answerFull = false;
  /** A response body, with the server's facts_hash when it advertises one. */
  const withFacts = (body: object) => (advertiseFacts ? { ...body, facts_hash: heldFactsHash } : body);

  function desiredStateFor(currentHash: string | null): { code: number; body: any } {
    const states = {
      good: {
        tenant_id: FLEET_TENANT_ID,
        state_hash: "h-good",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-good",
            name: "demo",
            version: "1.0",
            sha256: goodSha,
            format: "plain",
            signature: signBundle(signingKeys.privateKey, { id: "b-good", name: "demo", version: "1.0", sha256: goodSha }),
            url: "/agent/v1/bundles/b-good",
            priority: 100,
          },
        ],
      },
      evil: {
        tenant_id: FLEET_TENANT_ID,
        state_hash: "h-evil",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-evil",
            name: "evil",
            version: "6.6.6",
            sha256: evilSha,
            format: "plain",
            signature: signBundle(wrongKeys.privateKey, { id: "b-evil", name: "evil", version: "6.6.6", sha256: evilSha }),
            url: "/agent/v1/bundles/b-evil",
            priority: 100,
          },
        ],
      },
      trimmed: {
        tenant_id: FLEET_TENANT_ID,
        state_hash: "h-trimmed",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-trimmed",
            name: "demo",
            version: "2.0",
            sha256: trimmedSha,
            format: "plain",
            signature: signBundle(signingKeys.privateKey, { id: "b-trimmed", name: "demo", version: "2.0", sha256: trimmedSha }),
            url: "/agent/v1/bundles/b-trimmed",
            priority: 100,
          },
        ],
      },
      // A desired state whose bundle download 403s: the server recomputed
      // membership after handing out the state, so the state is stale.
      gone: {
        tenant_id: FLEET_TENANT_ID,
        state_hash: "h-gone",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-gone",
            name: "demo",
            version: "3.0",
            sha256: goodSha,
            format: "plain",
            signature: signBundle(signingKeys.privateKey, { id: "b-gone", name: "demo", version: "3.0", sha256: goodSha }),
            url: "/agent/v1/bundles/b-gone",
            priority: 100,
          },
        ],
      },
      facts: {
        tenant_id: FLEET_TENANT_ID,
        state_hash: "h-facts",
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [
          {
            id: "b-facts",
            name: "inventory",
            version: "1.0",
            sha256: factsSha,
            format: "plain",
            signature: signBundle(signingKeys.privateKey, { id: "b-facts", name: "inventory", version: "1.0", sha256: factsSha }),
            url: "/agent/v1/bundles/b-facts",
            priority: 100,
          },
        ],
      },
    };
    const active = states[phase];
    if (currentHash === active.state_hash && !answerFull) {
      return { code: 304, body: { next_poll_in_seconds: 1 } };
    }
    return { code: 200, body: withFacts(active) };
  }

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    // A tag producer: on Windows CheckDisk publishes `drives=c:,...` into the
    // central tag repository at load, which must surface in reported_tags.
    // Where the build has no CheckDisk (macOS, until it is ported) a module
    // it does carry stands in, so the "local configuration present" half of
    // the scenario still runs on a real module rather than a load error.
    await nscp.configure({ "/modules": { [LOCAL_MODULE]: "enabled" } });
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
        requests.push({ method: req.method ?? "", url: req.url ?? "", body, raw });

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
        } else if (parsed.pathname === "/agent/v1/bundles/b-facts") {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(factsZip);
        } else if (req.method === "POST" && parsed.pathname === "/agent/v1/facts") {
          if (factsStatus === 200) heldFactsHash = body?.facts_hash ?? "";
          res.writeHead(factsStatus, { "Content-Type": "application/json" });
          res.end(factsStatus === 200 ? "{}" : JSON.stringify({ error: "not found" }));
        } else if (parsed.pathname === "/agent/v1/state-report") {
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end(JSON.stringify(withFacts({})));
        } else if (parsed.pathname === "/agent/v1/bundles/b-gone") {
          res.writeHead(403, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "bundle no longer in effective set" }));
        } else if (parsed.pathname === "/agent/v1/renew") {
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
  const factsUploads = () => requests.filter((r) => r.url === "/agent/v1/facts");
  const polls = () => requests.filter((r) => r.url.startsWith("/agent/v1/desired-state")).length;
  /** Let the loop run `count` more poll cycles, so anything it was going to send has been sent. */
  async function settle(count = 3): Promise<void> {
    const target = polls() + count;
    await waitFor(`${count} more polls`, () => polls() >= target);
  }

  it("enrolls, writing the manifest and the fleet.ini include (no module needed)", async () => {
    const r = await nscp.run(["enroll", "--server", baseUrl, "--token", "tok-fleet", "--insecure"], {
      allowFailure: true,
    });
    expect(r.exitCode).toBe(0);
    // The manifest is the activation switch: its presence is what makes the
    // core start the sync thread at boot.
    expect(fs.existsSync(path.join(workDir, "security", "agent-state.json"))).toBe(true);

    const ini = fs.readFileSync(nscp.settingsFile, "utf8");
    // The sync loop lives in the core; enrollment must NOT enable any module.
    expect(ini).not.toContain("NSClientConfig");
    expect(ini).toMatch(/\[\/includes\]/);
    // Written as the ${fleet-folder} token, unexpanded and identical on every
    // platform, so relocating the folder moves the include with it.
    expect(ini).toMatch(/fleet\s*=\s*\$\{fleet-folder\}\/fleet\.ini/);
    // The include target exists as a placeholder so boot never logs a
    // missing-file error before the first sync.
    expect(fs.existsSync(path.join(workDir, "fleet", "fleet.ini"))).toBe(true);
  });

  it("syncs desired state: verifies, applies and reports", async () => {
    nscp.start();

    // The loop applies the good state and reports the hash.
    await waitFor("state report with applied hash", () =>
      stateReports().some((r) => r.body?.applied_state_hash === "h-good"),
    );

    const report = stateReports().find((r) => r.body?.applied_state_hash === "h-good")!;
    expect(report.body.bundles_installed).toEqual([{ id: "b-good", version: "1.0" }]);
    expect(report.body.errors).toEqual([]);
    expect(report.body.reported_tags.os).toBeTruthy();
    // Whether the host has configuration of its own that outranks what we just
    // applied. This suite enables a module locally, so it does - and the report
    // still carries no hint of *what* is configured.
    expect(report.body.local_config_present).toBe(true);
    expect(JSON.stringify(report.body)).not.toContain(LOCAL_MODULE);
    // Nothing is enabled, so every report carries the hash of the empty
    // document - and nothing was uploaded, because there is nothing to send.
    for (const r of stateReports()) expect(r.body.facts_hash).toBe(EMPTY_FACTS_HASH);
    expect(factsUploads()).toEqual([]);
    if (onWindows) {
      // Module-contributed tags (CheckDisk's drive list) ride along in every
      // state report, merged from the central tag repository.
      expect(report.body.reported_tags.drives).toMatch(/^[a-z]:(,[a-z]:)*$/);
    }

    // Rendered INI: merged JSON -> sections, deterministic.
    const fleetIni = fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
    expect(fleetIni).toContain("[/modules]");
    expect(fleetIni).toContain("CheckHelpers=enabled");
    expect(fleetIni).toContain("[/settings/fleet demo]");
    expect(fleetIni).toContain("greeting=hello");
    expect(fleetIni).toContain("retries=3");

    // Scripts staged under the managed path; bundle cached by id+sha.
    expect(
      fs.readFileSync(path.join(workDir, "fleet", "scripts", "demo", "hello.txt"), "utf8"),
    ).toContain("hello fleet");
    expect(
      fs.existsSync(path.join(workDir, "fleet", "cache", `b-good-${goodSha.substring(0, 16)}.zip`)),
    ).toBe(true);

    // Applied hash persisted for restarts.
    const applied = JSON.parse(
      fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"),
    );
    expect(applied.state_hash).toBe("h-good");

    // Startup calls happened: heartbeat + an early tag report before the poll.
    expect(requests.some((r) => r.url === "/agent/v1/heartbeat")).toBe(true);
    const firstDesired = requests.findIndex((r) => r.url.startsWith("/agent/v1/desired-state"));
    const firstReport = requests.findIndex((r) => r.url.startsWith("/agent/v1/state-report"));
    expect(firstReport).toBeGreaterThanOrEqual(0);
    expect(firstReport).toBeLessThan(firstDesired);
  });

  it("settles into 304 polling with the applied hash", async () => {
    await waitFor("a 304 steady-state poll", () =>
      requests.some((r) => r.url === "/agent/v1/desired-state?current_hash=h-good"),
    );
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
    const applied = JSON.parse(
      fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"),
    );
    expect(applied.state_hash).toBe("h-good");
    // The unverified bundle must never enter the cache.
    expect(
      fs.existsSync(path.join(workDir, "fleet", "cache", `b-evil-${evilSha.substring(0, 16)}.zip`)),
    ).toBe(false);
  });

  it("drops scripts that left the desired state", async () => {
    const scripts = path.join(workDir, "fleet", "scripts", "demo");
    expect(fs.existsSync(path.join(scripts, "hello.txt"))).toBe(true);

    phase = "trimmed";
    await waitFor("state report with the trimmed hash", () =>
      stateReports().some((r) => r.body?.applied_state_hash === "h-trimmed"),
    );

    // The new bundle's script is there and the one it replaced is gone: a
    // script removed from a bundle must not linger and keep running.
    expect(fs.readFileSync(path.join(scripts, "other.txt"), "utf8")).toContain("other fleet");
    expect(fs.existsSync(path.join(scripts, "hello.txt"))).toBe(false);
  });

  it("abandons a stale desired state (bundle 403) without reporting a failure", async () => {
    const reportsBefore = stateReports().length;
    const goneFetches = () => requests.filter((r) => r.url === "/agent/v1/bundles/b-gone").length;
    phase = "gone";

    // The state was handed out but its bundle 403s: the desired state changed
    // server-side mid-cycle. Wait for a couple of full poll cycles so a
    // failure report would have had every chance to show up.
    await waitFor("two attempts at the withdrawn bundle", () => goneFetches() >= 2);

    // Stale is not a configuration failure: nothing is reported, and the
    // previously applied configuration stays untouched.
    const since = stateReports().slice(reportsBefore);
    expect(since.filter((r) => r.body?.errors?.length > 0)).toEqual([]);
    expect(since.filter((r) => r.body?.applied_state_hash === "h-gone")).toEqual([]);
    const applied = JSON.parse(
      fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"),
    );
    expect(applied.state_hash).toBe("h-trimmed");

    // Once the server serves a consistent state again, the loop applies it.
    phase = "good";
    await waitFor("the replacement state to be applied", () =>
      stateReports()
        .slice(reportsBefore)
        .some((r) => r.body?.applied_state_hash === "h-good"),
    );
  });
  it("uploads the inventory once after a bundle enables a fact set", async () => {
    expect(factsUploads()).toEqual([]);
    phase = "facts";

    // The bundle loads CheckSystem and turns `os` on; the reload round puts
    // the set in the document and the loop uploads it.
    await waitFor("a facts upload carrying os", () => factsUploads().some((r) => r.body?.facts?.os));
    const upload = factsUploads()[0];
    expect(factsUploads()).toHaveLength(1);
    expect(upload.body.facts.os.family).toBe(onWindows ? "windows" : "linux");
    expect(upload.body.collected_at).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$/);
    // The hash is the digest of the document bytes exactly as they were sent,
    // which is what lets the server check it without a canonical re-encoding
    // of its own.
    const start = upload.raw.indexOf('"facts":') + '"facts":'.length;
    const end = upload.raw.lastIndexOf(',"facts_hash":');
    expect(upload.body.facts_hash).toBe(crypto.createHash("sha256").update(upload.raw.slice(start, end), "utf8").digest("hex"));
    expect(upload.body.facts_hash).not.toBe(EMPTY_FACTS_HASH);

    // An unchanged document is not sent again.
    await settle();
    expect(factsUploads()).toHaveLength(1);
  });

  it("re-uploads when the server says it holds something else", async () => {
    const ours = factsUploads()[0].body.facts_hash;
    // The server lost the document: it says so in the full answer to a poll.
    // The agent re-applies the unchanged state (no reload, nothing rendered
    // differently) and reports it, carrying our hash, never the document.
    heldFactsHash = "0".repeat(64);
    advertiseFacts = true;
    answerFull = true;

    await waitFor("a second facts upload", () => factsUploads().length >= 2);
    expect(factsUploads()[1].body.facts_hash).toBe(ours);
    await waitFor("a state report with our facts hash", () => stateReports().some((r) => r.body?.facts_hash === ours));
    for (const r of stateReports()) expect(r.body).not.toHaveProperty("facts");

    // The server now holds ours and says so on every poll: nothing more to send.
    expect(heldFactsHash).toBe(ours);
    await settle();
    expect(factsUploads()).toHaveLength(2);
  });

  it("stays quiet against a server that predates the facts call", async () => {
    await nscp.stop();
    advertiseFacts = false;
    factsStatus = 404;
    const before = factsUploads().length;
    const reportsBefore = stateReports().length;
    nscp.start();

    // After a restart the agent does not know what the server holds, so it
    // offers its inventory once. The 404 says the server has no such call.
    await waitFor("the startup facts upload", () => factsUploads().length > before);
    await settle(5);
    expect(factsUploads()).toHaveLength(before + 1);
    // Everything else carries on, the hash included.
    expect(stateReports().length).toBeGreaterThan(reportsBefore);
    expect(stateReports()[stateReports().length - 1].body.facts_hash).toBe(factsUploads()[before].body.facts_hash);

    // A server that starts talking about facts (it was upgraded) gets them.
    factsStatus = 200;
    heldFactsHash = "";
    advertiseFacts = true;
    await waitFor("an upload once the server speaks facts", () => factsUploads().length > before + 1);
    expect(factsUploads()[before + 1].body.facts.os).toBeTruthy();
    answerFull = false;
  });
});
