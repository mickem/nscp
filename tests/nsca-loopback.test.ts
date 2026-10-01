/**
 * NSCAClient -> NSCAServer inside one agent: every cipher and payload length
 * the client can speak has to be read back by our own server.
 *
 * Port of scripts/python/test_nsca.py. The legacy test registered a Python
 * subscription on the server's inbox and asserted on what it saw. Here the
 * inbox is the web server's passive result cache: NSCAServer posts every
 * packet it decodes onto `CHANNEL`, WEBServer caches whatever arrives there
 * keyed by the service name (rest-results.test.ts proves the cache itself),
 * and the test reads it back with GET /api/v2/results/<key>. Each submission
 * uses a fresh UUID as its service name, so a result can only be found if
 * that exact packet made it through encryption, the wire and decryption.
 *
 * Two submission paths, as in the legacy test:
 *   - submit_nsca over REST, against the configured `valid` target and against
 *     an unconfigured target whose address, cipher, key and length all come
 *     from the request (the legacy `test_rp` target);
 *   - the `nscp nsca` CLI, a second process speaking to the agent's listener.
 *
 * Both modules are reconfigured for each cipher and length through
 * PUT /api/v2/settings and a reload of the running modules, which is the
 * loadModuleEx re-entry rule from CLAUDE.md exercised once per matrix row.
 * A Lua `reload_modules` query does the reload; from inside a dispatched call
 * the core defers it to the scheduler, so every reconfiguration is confirmed
 * by a probe submission landing rather than by a sleep.
 *
 * The default matrix is every cipher at the protocol's 512 bytes plus every
 * length at aes256 (the default) and xor. NSCP_FULL_MATRIX=1 runs all 64
 * combinations.
 *
 * nsca-ciphers.test.ts tests a different property and stays: the `nscp nsca`
 * CLI against a real libmcrypt nsca daemon. There 3way and gost fail
 * (libmcrypt and nscp disagree on them), while here, with nscp on both ends,
 * they round-trip.
 */
import * as fs from "fs";
import * as path from "path";
import { randomUUID } from "crypto";

import request from "supertest";

import {
  CRITICAL,
  NscpInstance,
  OK,
  REST_URL,
  UNKNOWN,
  WARNING,
  executeQuery,
  messageOf,
  putSettings,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(600_000);

/** NSCA listener for this suite, off the 5667 default and the other suites' ports. */
const NSCA_PORT = 25667;
/** Nothing listens here. */
const DEAD_PORT = 25668;
/** The channel NSCAServer posts to and WEBServer caches from. */
const CHANNEL = "nsca_loopback";
/** What the client claims to be; the server passes it through as the result's host. */
const SENDER = "nsca-loopback-sender";
const SERVER_PATH = "/settings/NSCA/server";
const TARGET_PATH = "/settings/NSCA/client/targets/valid";

const CIPHERS = [
  "none",
  "xor",
  "des",
  "3des",
  "cast128",
  "xtea",
  "blowfish",
  "twofish",
  "rc2",
  "aes",
  "aes256",
  "aes192",
  "aes128",
  "serpent",
  "gost",
  "3way",
];
const LENGTHS = [128, 512, 1024, 4096];

const fullMatrix = process.env.NSCP_FULL_MATRIX === "1";

/** [cipher, payload length] rows, in run order. */
const MATRIX: Array<[string, number]> = fullMatrix
  ? CIPHERS.flatMap((c) => LENGTHS.map((l): [string, number] => [c, l]))
  : [
      ...CIPHERS.map((c): [string, number] => [c, 512]),
      ...["aes256", "xor"].flatMap((c) =>
        LENGTHS.filter((l) => l !== 512).map((l): [string, number] => [c, l]),
      ),
    ];

const STATES: Array<[string, number]> = [
  ["ok", OK],
  ["warning", WARNING],
  ["critical", CRITICAL],
  ["unknown", UNKNOWN],
];

/** The key the server is configured with for a cipher; the client must match it. */
const keyFor = (cipher: string) => `pwd-${cipher}`;

const LUA = `
-- Reload the named modules (REST passes a parameter with no value as a bare
-- argument). From inside a REST call the core hands the reload to the
-- scheduler, so this returns before it has applied.
local function reload_modules(command, args)
  local core = Core()
  for _, m in ipairs(args) do core:reload(m) end
  return 'ok', 'reload requested: ' .. table.concat(args, ',')
end
Registry():simple_function('reload_modules', reload_modules, 'reload the named modules')
`;

interface CachedResult {
  key: string;
  command: string;
  status: number;
  message: string;
  host: string;
  channel: string;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("NSCA client -> server loopback", () => {
  let nscp: NscpInstance;
  let key: string;

  /** The cached result for one service name, or undefined if it never arrived. */
  async function cached(service: string): Promise<CachedResult | undefined> {
    const res = await request(REST_URL)
      .get(`/api/v2/results/${encodeURIComponent(service)}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    if (res.status === 404) return undefined;
    expect(res.status).toBe(200);
    return res.body as CachedResult;
  }

  /** Wait for a submission to land; undefined once `timeoutMs` has passed. */
  async function waitFor(service: string, timeoutMs = 5_000): Promise<CachedResult | undefined> {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const r = await cached(service);
      if (r || Date.now() >= deadline) return r;
      await sleep(100);
    }
  }

  /** submit_nsca over REST; returns the submission's own status and message. */
  async function submit(
    service: string,
    state: string,
    message: string,
    extra: Record<string, string> = {},
  ): Promise<{ result: number; message: string }> {
    const q = await executeQuery(key, "submit_nsca", {
      command: service,
      result: state,
      message,
      retries: "0",
      ...extra,
    });
    return { result: q.result, message: messageOf(q) };
  }

  /** Request options that make an unconfigured target speak to our listener. */
  function adHoc(cipher: string, length: number, port = NSCA_PORT): Record<string, string> {
    return {
      target: "test_rp",
      address: `127.0.0.1:${port}`,
      encryption: cipher,
      password: keyFor(cipher),
      "payload-length": String(length),
    };
  }

  /** How many times the core has started reloading `module` so far. */
  const reloadsOf = (module: string) =>
    nscp
      .capturedStdout()
      .split(/\r?\n/)
      .filter((l) => l.includes(`Reloading: ${module}`)).length;

  /**
   * Ask for a reload of `modules` and wait until the core has run it. From
   * inside a REST call the core defers a reload to its scheduler, so the call
   * returns before anything has happened; the core logs `Reloading: <module>`
   * when it gets to it.
   */
  async function reloadModules(...modules: string[]): Promise<void> {
    const before = modules.map(reloadsOf);
    const r = await executeQuery(
      key,
      "reload_modules",
      Object.fromEntries(modules.map((m) => [m, ""])),
    );
    expect(r.result).toBe(OK);
    const deadline = Date.now() + 60_000;
    while (modules.some((m, i) => reloadsOf(m) <= before[i])) {
      if (Date.now() >= deadline)
        throw new Error(`${modules.join(", ")} did not reload within 60s`);
      await sleep(100);
    }
  }

  /** What both ends were last switched to; the boot configuration first. */
  let current = { cipher: "aes256", length: 512 };

  /**
   * Point both modules at one cipher, key and length, reload them, and wait
   * until the change has applied.
   *
   * Once both reloads have run, a probe goes out with request-supplied
   * options carrying the new cipher, key and length, which only the new
   * server can decode, and one through the configured target, which only
   * matches the new server if the client re-read the target. A switch to what
   * is already configured is skipped.
   */
  async function reconfigure(cipher: string, length: number): Promise<void> {
    if (current.cipher === cipher && current.length === length) return;
    await putSettings(key, SERVER_PATH, {
      encryption: cipher,
      password: keyFor(cipher),
      "payload length": length,
    });
    await putSettings(key, TARGET_PATH, {
      encryption: cipher,
      password: keyFor(cipher),
      "payload length": length,
    });
    await reloadModules("NSCAServer", "NSCAClient");

    const deadline = Date.now() + 30_000;
    let direct: CachedResult | undefined;
    let configured: CachedResult | undefined;
    while (!(direct && configured)) {
      if (Date.now() >= deadline) {
        throw new Error(
          `NSCA loopback did not come up with ${cipher}/${length} within 30s ` +
            `(request options: ${direct ? "landed" : "lost"}, configured target: ${configured ? "landed" : "lost"})`,
        );
      }
      const a = `probe-${randomUUID()}`;
      const b = `probe-${randomUUID()}`;
      await submit(a, "ok", "probe", adHoc(cipher, length));
      await submit(b, "ok", "probe", { target: "valid" });
      direct = direct ?? (await waitFor(a, 1_000));
      configured = configured ?? (await waitFor(b, 1_000));
    }
    current = { cipher, length };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    const script = path.join(nscp.scratch("lua"), "nsca_loopback.lua");
    fs.writeFileSync(script, LUA);

    await nscp.waitForPortFree(NSCA_PORT, { timeoutMs: 30_000 });
    key = await setupQueryNscp(nscp, "LUAScript", {
      "/modules": {
        LUAScript: "enabled",
        WEBServer: "enabled",
        NSCAServer: "enabled",
        NSCAClient: "enabled",
      },
      "/settings/lua/scripts": { nsca_loopback: script },
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        // The service name, which is a fresh UUID per submission.
        "primary index": "${command}",
        "clear on poll": "false",
      },
      [SERVER_PATH]: {
        port: String(NSCA_PORT),
        inbox: CHANNEL,
        encryption: "aes256",
        password: keyFor("aes256"),
        "payload length": "512",
      },
      "/settings/NSCA/client": { hostname: SENDER },
      [TARGET_PATH]: {
        address: `127.0.0.1:${NSCA_PORT}`,
        encryption: "aes256",
        password: keyFor("aes256"),
        "payload length": "512",
      },
      // Configured with the wrong key: the client cannot tell, the server
      // cannot decrypt.
      "/settings/NSCA/client/targets/wrong_key": {
        address: `127.0.0.1:${NSCA_PORT}`,
        encryption: "aes256",
        password: "not-the-key",
        "payload length": "512",
      },
      // Right key, twice the server's length: the packet does not parse.
      "/settings/NSCA/client/targets/wrong_length": {
        address: `127.0.0.1:${NSCA_PORT}`,
        encryption: "aes256",
        password: keyFor("aes256"),
        "payload length": "1024",
      },
      "/settings/NSCA/client/targets/dead": {
        address: `127.0.0.1:${DEAD_PORT}`,
        encryption: "aes256",
        password: keyFor("aes256"),
        "payload length": "512",
      },
    });
    await nscp.waitForPort(NSCA_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe.each(MATRIX)("%s at %i bytes", (cipher, length) => {
    beforeAll(async () => {
      await reconfigure(cipher, length);
    });

    it.each(STATES)("submit_nsca to the configured target carries %s", async (state, code) => {
      const service = randomUUID();
      const message = `${state} via ${cipher}/${length}`;
      const sent = await submit(service, state, message, { target: "valid" });
      expect(sent).toEqual({ result: OK, message: "Submission successful" });

      const got = await waitFor(service);
      expect(got).toBeDefined();
      expect(got?.status).toBe(code);
      expect(got?.message).toBe(message);
      expect(got?.host).toBe(SENDER);
      expect(got?.channel).toBe(CHANNEL);
    });

    it.each(STATES)(
      "submit_nsca with the target in the request carries %s",
      async (state, code) => {
        const service = randomUUID();
        const message = `${state} via request options`;
        const sent = await submit(service, state, message, adHoc(cipher, length));
        expect(sent).toEqual({ result: OK, message: "Submission successful" });

        const got = await waitFor(service);
        expect(got?.status).toBe(code);
        expect(got?.message).toBe(message);
      },
    );

    it("the nscp nsca CLI reaches the listener from another process", async () => {
      const service = randomUUID();
      const message = `critical via the CLI`;
      const r = await nscp.run([
        "nsca",
        `--host=127.0.0.1`,
        `--port=${NSCA_PORT}`,
        `--encryption=${cipher}`,
        `--password=${keyFor(cipher)}`,
        `--payload-length=${length}`,
        `--command=${service}`,
        "--result=2",
        `--message=${message}`,
        "--retries=0",
      ]);
      expect(r.exitCode).toBe(0);
      expect(r.stdout).toMatch(/Submission successful/);

      const got = await waitFor(service);
      expect(got?.status).toBe(CRITICAL);
      expect(got?.message).toBe(message);
    });
  });

  describe("submissions that must not land", () => {
    beforeAll(async () => {
      // The misconfigured targets above are written for aes256 at 512 bytes.
      await reconfigure("aes256", 512);
    });

    it("a wrong key is accepted by the client and dropped by the server", async () => {
      const service = randomUUID();
      const sent = await submit(service, "critical", "forged", { target: "wrong_key" });
      // NSCA has no acknowledgement: the client only knows the bytes left.
      expect(sent).toEqual({ result: OK, message: "Submission successful" });
      expect(await waitFor(service, 3_000)).toBeUndefined();
    });

    it("a payload length the server does not use is dropped", async () => {
      const service = randomUUID();
      const sent = await submit(service, "critical", "too long", { target: "wrong_length" });
      expect(sent.result).toBe(OK);
      expect(await waitFor(service, 3_000)).toBeUndefined();
    });

    it("an unreachable server is reported as a failed submission", async () => {
      const service = randomUUID();
      const sent = await submit(service, "critical", "nobody home", { target: "dead" });
      expect(sent.result).toBe(UNKNOWN);
      expect(sent.message).toMatch(/error/i);
    });

    it("the agent still accepts submissions afterwards", async () => {
      const service = randomUUID();
      await submit(service, "ok", "still here", { target: "valid" });
      expect((await waitFor(service))?.status).toBe(OK);
    });
  });

  describe("reloading with a new key", () => {
    it("drops the old key and accepts the new one", async () => {
      await reconfigure("aes256", 512);
      await reconfigure("serpent", 1024);

      // The old cipher and key, as a host that was not told would still send.
      const stale = randomUUID();
      await submit(stale, "critical", "old key", adHoc("aes256", 512));
      expect(await waitFor(stale, 3_000)).toBeUndefined();

      const fresh = randomUUID();
      await submit(fresh, "warning", "new key", adHoc("serpent", 1024));
      expect((await waitFor(fresh))?.status).toBe(WARNING);
    });

    it("refuses to start the listener on a reload that removes the key", async () => {
      await putSettings(key, SERVER_PATH, { encryption: "aes256", password: "" });
      await reloadModules("NSCAServer");

      // The refusal stops the old listener and starts no new one.
      const deadline = Date.now() + 30_000;
      let refused = false;
      while (!refused && Date.now() < deadline) {
        refused = /Refusing to start NSCA server/.test(nscp.capturedStdout());
        if (!refused) await sleep(250);
      }
      expect(refused).toBe(true);

      // ...and putting the key back brings it up again.
      await reconfigure("aes256", 512);
    });
  });
});
