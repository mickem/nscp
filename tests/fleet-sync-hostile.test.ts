/**
 * Adversarial coverage of the fleet apply pipeline.
 *
 * fleet-sync.test.ts drives the happy path plus one tampered bundle. This
 * suite is the negative half: everything a compromised (or badly broken) fleet
 * server can put in a desired state or a bundle, driven through the real apply
 * path in the core service. Each case asserts three things, because any one of
 * them alone is a hole:
 *
 *   1. the apply FAILS (no partial application),
 *   2. the failure is REPORTED without an applied_state_hash (the server must
 *      not believe the host is in the new state), and
 *   3. the previously applied configuration and the filesystem OUTSIDE the
 *      managed directory are untouched.
 *
 * The last test drives a good state again: refusing hostile input must not
 * wedge the agent.
 *
 * The bundle signing key here is the real one, so every hostile bundle is
 * correctly signed - these cases test what happens AFTER signature
 * verification, which is exactly where signing stops helping.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import { NscpInstance, makeZip, bundleEntry, cacheFileName, makeCertPem, sha256Hex, signBundle } from "@fixtures/index";

jest.setTimeout(180_000);

interface SeenRequest {
  method: string;
  url: string;
  body: any;
}

describe("fleet sync against a hostile server", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let outsideDir: string;
  let server: http.Server;
  let baseUrl: string;
  let requests: SeenRequest[];

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  /** The baseline good bundle every hostile case is compared against. */
  const goodZip = makeZip([
    { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" } }) },
    { name: "scripts/demo/keep.txt", data: "keep me\n" },
  ]);

  /** Escapes: relative traversal, absolute paths, backslashes, drive letters. */
  const traversalZip = makeZip([
    { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" } }) },
    { name: "scripts/../../../../escaped-relative.txt", data: "pwned\n" },
  ]);
  const absoluteZip = makeZip([{ name: "/etc/nscp-escaped-absolute.txt", data: "pwned\n" }]);
  const backslashZip = makeZip([{ name: "scripts\\..\\..\\escaped-backslash.txt", data: "pwned\n" }]);
  const driveZip = makeZip([{ name: "scripts/C:/escaped-drive.txt", data: "pwned\n" }]);
  const dotdotNameZip = makeZip([{ name: "scripts/..evil..txt", data: "pwned\n" }]);

  /** A bundle whose config.json smuggles INI lines into a value. */
  const iniInjectionZip = makeZip([
    {
      name: "config.json",
      data: JSON.stringify({
        settings: { "fleet demo": { greeting: "hi\n[/settings/external scripts/scripts]\npwned=/bin/sh -c id" } },
      }),
    },
  ]);

  /** A bundle claiming a huge uncompressed size (the zip-bomb guard). */
  // Honest about its size, and over the 16MB per-file limit: this is the case
  // the guard exists for, and every zip backend reports it the same way.
  const bombZip = makeZip([
    { name: "config.json", data: JSON.stringify({}) },
    { name: "scripts/bomb.txt", data: Buffer.alloc(20 * 1024 * 1024, 0x41) },
  ]);

  /**
   * The same idea but lying: 13 bytes claiming to be 512MB. Which check stops
   * it depends on the zip backend - a strict libzip refuses to open an archive
   * whose declared size does not match, a lenient one gets as far as our size
   * guard - so this case only asserts that it IS stopped.
   */
  const lyingBombZip = makeZip([
    { name: "config.json", data: JSON.stringify({}) },
    { name: "scripts/bomb.txt", data: "small on disk", declaredSize: 512 * 1024 * 1024 },
  ]);

  /** A bundle with an unparseable config.json. */
  const badConfigZip = makeZip([{ name: "config.json", data: "{not json" }]);

  /** A bundle that is not a zip at all (but is correctly signed). */
  const notAZip = Buffer.from("this is definitely not a zip archive");

  /** Poisoned-cache case: the desired state advertises this content... */
  const cachedZip = makeZip([{ name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" } }) }]);

  const zips: Record<string, Buffer> = {
    good: goodZip,
    traversal: traversalZip,
    absolute: absoluteZip,
    backslash: backslashZip,
    drive: driveZip,
    dotdot: dotdotNameZip,
    inject: iniInjectionZip,
    bomb: bombZip,
    lyingbomb: lyingBombZip,
    badconfig: badConfigZip,
    notzip: notAZip,
    cached: cachedZip,
  };

  /** What the server serves right now, and how. */
  let phase = "good";
  /** Status to answer bundle downloads with (200 = serve the bundle). */
  let bundleStatus = 200;
  /** Raw override for the desired-state body (for malformed-payload cases). */
  let rawDesiredState: { code: number; body: string } | undefined;

  const stateHashes: Record<string, string> = {
    good: "h-good",
    traversal: "h-traversal",
    absolute: "h-absolute",
    backslash: "h-backslash",
    drive: "h-drive",
    dotdot: "h-dotdot",
    inject: "h-inject",
    bomb: "h-bomb",
    lyingbomb: "h-lyingbomb",
    badconfig: "h-badconfig",
    notzip: "h-notzip",
    cached: "h-cached",
  };

  function desiredStateFor(currentHash: string | null): { code: number; body: string } {
    if (rawDesiredState) return rawDesiredState;
    const zip = zips[phase];
    const hash = stateHashes[phase];
    if (currentHash === hash) {
      return { code: 304, body: JSON.stringify({ next_poll_in_seconds: 1 }) };
    }
    return {
      code: 200,
      body: JSON.stringify({
        state_hash: hash,
        next_poll_in_seconds: 1,
        merged_config_json: {},
        bundles: [bundleEntry(`b-${phase}`, zip, signingKeys.privateKey)],
      }),
    };
  }

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-hostile-"));
    outsideDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-outside-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    requests = [];

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
          res.end(r.body);
        } else if (parsed.pathname.startsWith("/agent/v1/bundles/")) {
          if (bundleStatus !== 200) {
            res.writeHead(bundleStatus);
            res.end("no");
            return;
          }
          const id = parsed.pathname.substring("/agent/v1/bundles/".length);
          const zip = zips[id.replace(/^b-/, "")];
          if (!zip) {
            res.writeHead(404);
            res.end("unknown bundle");
            return;
          }
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(zip);
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
  });

  afterAll(async () => {
    await nscp.stop();
    await new Promise<void>((resolve) => server.close(() => resolve()));
    fs.rmSync(outsideDir, { recursive: true, force: true });
  });

  async function waitFor(what: string, predicate: () => boolean, timeoutMs = 45_000): Promise<void> {
    const started = Date.now();
    while (!predicate()) {
      if (Date.now() - started > timeoutMs) throw new Error(`Timed out waiting for ${what}`);
      await new Promise((r) => setTimeout(r, 150));
    }
  }

  const stateReports = () => requests.filter((r) => r.url.startsWith("/agent/v1/state-report"));
  const fleetIni = () => fs.readFileSync(path.join(workDir, "fleet", "fleet.ini"), "utf8");
  const appliedState = () => JSON.parse(fs.readFileSync(path.join(workDir, "fleet", "applied-state.json"), "utf8"));

  /**
   * Serve `next` and wait for the agent to report a failure for it. Returns the
   * reported error strings. Asserts the report carries no applied hash.
   */
  async function expectRejected(next: string, matcher: RegExp): Promise<string[]> {
    const before = stateReports().length;
    phase = next;
    await waitFor(`a failure report for ${next}`, () =>
      stateReports()
        .slice(before)
        .some((r) => Array.isArray(r.body?.errors) && r.body.errors.length > 0),
    );
    const failure = stateReports()
      .slice(before)
      .find((r) => r.body?.errors?.length > 0)!;
    expect(failure.body.applied_state_hash).toBeUndefined();
    const errors = failure.body.errors as string[];
    expect(errors.join(" | ")).toMatch(matcher);
    return errors;
  }

  /** Nothing may have been written outside the managed fleet directory. */
  function expectNothingEscaped() {
    for (const name of ["escaped-relative.txt", "escaped-backslash.txt", "escaped-drive.txt", "nscp-escaped-absolute.txt"]) {
      expect(fs.existsSync(path.join(workDir, name))).toBe(false);
      expect(fs.existsSync(path.join(os.tmpdir(), name))).toBe(false);
      expect(fs.existsSync(path.join("/etc", name))).toBe(false);
      expect(fs.existsSync(path.join(outsideDir, name))).toBe(false);
    }
    // Nor anywhere reachable by walking up from the scripts directory.
    for (const up of ["..", "../..", "../../.."]) {
      const dir = path.join(workDir, "fleet", "scripts", up);
      if (!fs.existsSync(dir)) continue;
      for (const entry of fs.readdirSync(dir)) {
        expect(entry).not.toMatch(/escaped/);
      }
    }
  }

  it("applies a good bundle first, as the baseline every rejection is measured against", async () => {
    expect((await nscp.run(["enroll", "--server", baseUrl, "--token", "tok-hostile"], { allowFailure: true })).exitCode).toBe(0);
    await nscp.configure({ "/settings/core": { "metrics interval": "1s" } });
    nscp.start();

    await waitFor("the baseline state report", () => stateReports().some((r) => r.body?.applied_state_hash === "h-good"));
    expect(fleetIni()).toContain("CheckHelpers=enabled");
    expect(fs.existsSync(path.join(workDir, "fleet", "scripts", "demo", "keep.txt"))).toBe(true);
  });

  it("refuses a bundle whose script path climbs out of the scripts directory", async () => {
    await expectRejected("traversal", /unsafe path/i);
    expectNothingEscaped();
    expect(fleetIni()).toContain("CheckHelpers=enabled");
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("refuses an absolute path, a backslash escape and a drive letter", async () => {
    // Every entry is checked, including ones outside scripts/ that would never
    // have been staged: an archive containing an absolute path is malformed,
    // and a backslash escape is normalised to "/" before the check so it
    // cannot slip past on a Unix agent.
    for (const hostile of ["absolute", "backslash", "drive"]) {
      await expectRejected(hostile, /unsafe path/i);
      expectNothingEscaped();
    }
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("refuses a file name merely containing .. (conservative, on purpose)", async () => {
    // "scripts/..evil..txt" cannot escape anything, but distinguishing it from
    // a real traversal is not worth the risk of being wrong.
    await expectRejected("dotdot", /unsafe path/i);
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("refuses managed configuration that tries to inject INI lines", async () => {
    const errors = await expectRejected("inject", /control character/i);
    expect(errors.join(" ")).not.toContain("/bin/sh");
    // The injected section must not exist anywhere in the rendered file.
    expect(fleetIni()).not.toContain("external scripts");
    expect(fleetIni()).not.toContain("pwned");
    expect(fleetIni()).toContain("CheckHelpers=enabled");
  });

  it("refuses a bundle whose scripts are over the size limit", async () => {
    await expectRejected("bomb", /size limit/i);
    expect(fs.existsSync(path.join(workDir, "fleet", "scripts", "bomb.txt"))).toBe(false);
  });

  it("refuses a bundle that lies about its uncompressed size", async () => {
    // Either the zip backend rejects the malformed archive or our size guard
    // does; what matters is that neither unpacks it.
    await expectRejected("lyingbomb", /size limit|not a readable zip/i);
    expect(fs.existsSync(path.join(workDir, "fleet", "scripts", "bomb.txt"))).toBe(false);
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("refuses a bundle with an unparseable config.json", async () => {
    await expectRejected("badconfig", /config\.json is invalid/i);
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("refuses correctly signed bytes that are not a zip archive", async () => {
    await expectRejected("notzip", /not a readable zip/i);
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("keeps the applied configuration when a bundle download fails", async () => {
    for (const status of [403, 404, 500]) {
      const before = requests.length;
      bundleStatus = status;
      phase = "cached";
      await waitFor(`a bundle download attempt answered ${status}`, () =>
        requests.slice(before).some((r) => r.url.startsWith("/agent/v1/bundles/")),
      );
      // Nothing was applied and the agent is still polling.
      expect(appliedState().state_hash).toBe("h-good");
      expect(fleetIni()).toContain("CheckHelpers=enabled");
      const pollsBefore = requests.filter((r) => r.url.startsWith("/agent/v1/desired-state")).length;
      await waitFor("another poll after the failed download", () => requests.filter((r) => r.url.startsWith("/agent/v1/desired-state")).length > pollsBefore);
    }
    bundleStatus = 200;
  });

  it("re-verifies a bundle that came from the on-disk cache", async () => {
    // Verifying only on download would make the cache a way in for anything
    // that can write a file: poison the cache entry the next state points at.
    const cacheDir = path.join(workDir, "fleet", "cache");
    fs.mkdirSync(cacheDir, { recursive: true });
    const poisoned = path.join(cacheDir, cacheFileName("b-cached", cachedZip));
    fs.writeFileSync(poisoned, makeZip([{ name: "config.json", data: JSON.stringify({ modules: { EvilModule: "enabled" } }) }]));

    const errors = await expectRejected("cached", /verification|checksum/i);
    expect(errors.join(" ")).toMatch(/b-cached/);
    expect(fleetIni()).not.toContain("EvilModule");
    expect(appliedState().state_hash).toBe("h-good");
  });

  it("rejects a desired state whose fields are hostile or malformed", async () => {
    const bodies: { name: string; body: any }[] = [
      { name: "not json", body: "{ this is not json" },
      { name: "empty body", body: "" },
      { name: "array root", body: "[]" },
      { name: "no state_hash", body: JSON.stringify({ next_poll_in_seconds: 1, bundles: [] }) },
      {
        name: "state_hash with a CRLF",
        body: JSON.stringify({ state_hash: "h\r\nX-Injected: 1", next_poll_in_seconds: 1, bundles: [] }),
      },
      {
        name: "bundle id with a traversal",
        body: JSON.stringify({
          state_hash: "h-badid",
          next_poll_in_seconds: 1,
          bundles: [{ ...bundleEntry("b-good", goodZip, signingKeys.privateKey), id: "../../../../etc/cron.d/x" }],
        }),
      },
      {
        name: "short sha256",
        body: JSON.stringify({
          state_hash: "h-badsha",
          next_poll_in_seconds: 1,
          bundles: [{ ...bundleEntry("b-good", goodZip, signingKeys.privateKey), sha256: "beef" }],
        }),
      },
      {
        name: "absolute bundle url",
        body: JSON.stringify({
          state_hash: "h-badurl",
          next_poll_in_seconds: 1,
          bundles: [{ ...bundleEntry("b-good", goodZip, signingKeys.privateKey), url: "http://127.0.0.1:1/evil.zip" }],
        }),
      },
      {
        name: "bundles as an object",
        body: JSON.stringify({ state_hash: "h-badbundles", next_poll_in_seconds: 1, bundles: {} }),
      },
      {
        name: "merged_config_json as a string",
        body: JSON.stringify({ state_hash: "h-badconf", next_poll_in_seconds: 1, merged_config_json: "{}", bundles: [] }),
      },
    ];

    try {
      for (const { name, body } of bodies) {
        const before = stateReports().length;
        rawDesiredState = { code: 200, body };
        await waitFor(`a failure report for "${name}"`, () =>
          stateReports()
            .slice(before)
            .some((r) => Array.isArray(r.body?.errors) && r.body.errors.length > 0),
        );
        const failure = stateReports()
          .slice(before)
          .find((r) => r.body?.errors?.length > 0)!;
        expect(failure.body.applied_state_hash).toBeUndefined();
        // The applied configuration is untouched and no bundle was fetched for
        // the rejected state.
        expect(appliedState().state_hash).toBe("h-good");
        expect(requests.some((r) => r.url.includes("cron.d"))).toBe(false);
        expect(requests.some((r) => r.url.includes("evil.zip"))).toBe(false);
      }
    } finally {
      // Leave the server usable for the recovery test even if a case failed.
      rawDesiredState = undefined;
    }
  });

  it("stays healthy: a good state after all of that still applies", async () => {
    // Everything above must be a refusal, not a wedged agent.
    const goodAgainZip = makeZip([
      { name: "config.json", data: JSON.stringify({ modules: { CheckHelpers: "enabled" }, settings: { recovered: { ok: "yes" } } }) },
      { name: "scripts/demo/recovered.txt", data: "recovered\n" },
    ]);
    zips.recovered = goodAgainZip;
    stateHashes.recovered = "h-recovered";

    phase = "recovered";
    await waitFor("the recovery state report", () => stateReports().some((r) => r.body?.applied_state_hash === "h-recovered"));
    expect(fleetIni()).toContain("[/settings/recovered]");
    expect(fs.readFileSync(path.join(workDir, "fleet", "scripts", "demo", "recovered.txt"), "utf8")).toContain("recovered");
    expect(appliedState().state_hash).toBe("h-recovered");
    expectNothingEscaped();
  });

  it("never wrote anything outside the managed directory, all suite long", () => {
    // Belt and braces: the enrollment manifest, the managed tree and the
    // settings file are the only things the agent may have created.
    const allowed = new Set(["fleet", "security", "nsclient.ini", "boot.ini"]);
    for (const entry of fs.readdirSync(workDir)) {
      expect(allowed.has(entry)).toBe(true);
    }
    expect(fs.readdirSync(outsideDir)).toEqual([]);
  });
});
