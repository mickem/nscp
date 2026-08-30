/**
 * End-to-end check against a REAL collectd daemon (collectd-daemon, not our own
 * decoder): proves NSClient++'s CollectdClient speaks the binary network
 * protocol well enough for an actual collectd to parse, type-check, and store
 * the value-lists we send.
 *
 * Topology:
 *   nscp (host process) --UDP collectd binary--> collectd `network` plugin
 *                                                 --> collectd `csv` plugin
 *                                                 --> /data/<host>/<plugin>/<type>
 *
 * collectd's csv plugin only writes a file once it has accepted a value-list —
 * i.e. parsed the packet AND matched the value count/types against types.db —
 * so the presence of /data/<host>/cpu-total/cpu-user-* is positive proof the
 * wire format is correct, not just self-consistent with our own decoder (that
 * is what collectd-submit.test.ts covers).
 *
 * One collectd container is shared; each sub-scenario runs its own nscp with a
 * distinct collectd `host` so their output lands in separate /data/<host>
 * trees. collectd's UDP port is published with a raw `docker run -p
 * ...:25826/udp` because testcontainers only maps TCP; dockerd's proxy
 * forwards the datagrams from loopback to the container, and everything is read
 * back through `docker exec`.
 */
import * as path from "path";
import execa from "execa";
import { NscpInstance, dockerOrSkip } from "@fixtures/index";

jest.setTimeout(600_000);

const IMAGE_TAG = "nscp_collectd_it";

/** Thin `docker ...` wrapper. */
async function docker(args: string[], opts: { reject?: boolean } = {}) {
  return execa("docker", args, { reject: opts.reject ?? true, all: true });
}

// Credentials matching Dockerfiles/entrypoints/collectd.auth (the AuthFile of
// the Sign/Encrypt listeners).
const AUTH_USER = "nscp";
const AUTH_PASSWORD = "nscp-integration-secret";

dockerOrSkip()("CollectD real-daemon integration", () => {
  let containerId = "";
  /** Mapped loopback port of the SecurityLevel None listener (25826). */
  let port = 0;
  /** Mapped loopback port of the SecurityLevel Sign listener (25827). */
  let signPort = 0;
  /** Mapped loopback port of the SecurityLevel Encrypt listener (25828). */
  let encryptPort = 0;

  /** Files collectd's csv plugin has written for `host` so far. */
  async function dataFiles(host: string): Promise<string[]> {
    const r = await docker(
      ["exec", containerId, "sh", "-c", `find /data/${host} -type f 2>/dev/null || true`],
      { reject: false },
    );
    return r.stdout
      .split("\n")
      .map((s) => s.trim())
      .filter(Boolean);
  }

  /** Poll collectd's output dir for `host` until `predicate` holds. */
  async function waitForFiles(
    host: string,
    predicate: (files: string[]) => boolean,
    timeoutMs = 90_000,
  ): Promise<string[]> {
    const deadline = Date.now() + timeoutMs;
    let last: string[] = [];
    while (Date.now() < deadline) {
      last = await dataFiles(host);
      if (predicate(last)) return last;
      await new Promise((r) => setTimeout(r, 1000));
    }
    return last;
  }

  beforeAll(async () => {
    // Build with `docker build` (cwd = tests/, so tests/.dockerignore keeps
    // node_modules/ out of the context) and a fixed tag we can run ourselves
    // with a published UDP port.
    await execa("docker", ["build", "-t", IMAGE_TAG, "-f", "Dockerfiles/collectd.Dockerfile", "."], {
      cwd: path.resolve(__dirname),
      all: true,
    });

    const run = await execa(
      "docker",
      ["run", "-d", "-p", "127.0.0.1::25826/udp", "-p", "127.0.0.1::25827/udp", "-p", "127.0.0.1::25828/udp", IMAGE_TAG],
      {
        all: true,
      },
    );
    containerId = run.stdout.trim();

    // Wait for "Initialization complete, entering read-loop." in the logs.
    const deadline = Date.now() + 30_000;
    let ready = false;
    while (Date.now() < deadline) {
      const logs = await docker(["logs", containerId], { reject: false });
      if (/entering read-loop/.test(`${logs.stdout}\n${logs.stderr}`)) {
        ready = true;
        break;
      }
      await new Promise((r) => setTimeout(r, 500));
    }
    if (!ready) {
      const logs = await docker(["logs", containerId], { reject: false });
      throw new Error(`collectd did not become ready. Logs:\n${logs.stdout}\n${logs.stderr}`);
    }

    // The published loopback UDP ports (docker assigns them; `docker port` reports them).
    const mappedPort = async (containerPort: string): Promise<number> => {
      const portOut = (await docker(["port", containerId, containerPort])).stdout.trim();
      const p = Number(portOut.split("\n")[0].split(":").pop());
      if (!Number.isInteger(p) || p <= 0) {
        throw new Error(`could not parse mapped UDP port for ${containerPort} from: ${portOut}`);
      }
      return p;
    };
    port = await mappedPort("25826/udp");
    signPort = await mappedPort("25827/udp");
    encryptPort = await mappedPort("25828/udp");
  });

  afterAll(async () => {
    if (containerId) await docker(["rm", "-f", containerId], { reject: false });
  });

  // The built-in default mapping is platform-specific (CheckSystem's metric
  // namespace differs by OS), so the expected collectd output differs too. nscp
  // runs as a host process, so process.platform decides which default branch is
  // exercised; the collectd receiver is the same Linux container either way.
  const isWindows = process.platform === "win32";
  const label = isWindows ? "default Windows mapping" : "default Linux mapping";
  // [regex, human description] for files that MUST be present for this platform.
  const required: Array<[RegExp, string]> = isWindows
    ? [
        [/\/cpu-total\/cpu-user-/, "cpu-total/cpu-user"],
        [/\/cpu-\d+\/cpu-user-/, "per-core cpu-<N>/cpu-user"],
        [/\/memory\/memory-available-/, "memory/memory-available"],
        [/\/memory-pagefile\/memory-used-/, "memory-pagefile/memory-used"],
        [/\/processes\/ps_count-/, "processes/ps_count"],
        [/\/uptime\/uptime-/, "uptime/uptime"],
      ]
    : [
        [/\/cpu-total\/cpu-user-/, "cpu-total/cpu-user"],
        [/\/cpu-\d+\/cpu-user-/, "per-core cpu-<N>/cpu-user"],
        [/\/memory\/memory-free-/, "memory/memory-free"],
        [/\/swap\/swap-used-/, "swap/swap-used"],
        [/\/uptime\/uptime-/, "uptime/uptime"],
      ];

  describe(label, () => {
    const HOST = "it-collectd-default";
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: HOST },
        "/settings/collectd/client/targets/default": { address: `127.0.0.1:${port}` },
      });
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("stores the platform's default value-lists via a real collectd", async () => {
      // Every default mapping for this platform produces a standard collectd
      // type, so a real collectd accepts and stores them only if our packets
      // are valid collectd binary protocol. The per-core file (cpu-<N>/...)
      // specifically exercises the `core`/`core_<N>` variable expansion.
      const files = await waitForFiles(HOST, (f) => required.every(([re]) => f.some((p) => re.test(p))));
      for (const [re, desc] of required) {
        expect({ desc, present: files.some((p) => re.test(p)) }).toEqual({ desc, present: true });
      }

      // The stored CSV must contain collectd's "epoch,value" header plus at
      // least one data line — confirming collectd decoded a value, not just a
      // header.
      const cpuFile = files.find((p) => /\/cpu-total\/cpu-user-/.test(p))!;
      const content = (await docker(["exec", containerId, "cat", cpuFile])).stdout;
      expect(content).toMatch(/^epoch,value/m);
      expect(content).toMatch(/^\d+(?:\.\d+)?,\S+$/m);
    });
  });

  describe("multi-value type", () => {
    const HOST = "it-collectd-mv";
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: HOST },
        // A custom mapping emitting the two-value collectd "ps_count" type
        // (processes + threads) from two metrics that exist on Linux. This
        // proves a real collectd accepts our multi-value encoding — it drops
        // value-lists whose value count doesn't match the type in types.db.
        "/settings/collectd/client/metrics": {
          "processes-/ps_count": "gauge:system.cpu.total.user,system.cpu.total.idle",
        },
        "/settings/collectd/client/targets/default": { address: `127.0.0.1:${port}` },
      });
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("accepts a two-value ps_count value-list", async () => {
      const files = await waitForFiles(HOST, (f) => f.some((p) => /\/processes\/ps_count-/.test(p)));
      expect(files.some((p) => /\/processes\/ps_count-/.test(p))).toBe(true);
    });
  });

  // --------------------------------------------------------------------------
  // Security levels: the Sign/Encrypt listeners only accept traffic carrying a
  // valid signature/encryption part for a user in the AuthFile. collectd
  // verifying and storing the value-lists is positive proof the client's
  // HMAC-SHA-256 / AES-256-OFB framing matches the reference implementation —
  // a wrong hash input, key derivation or layout is silently dropped instead.
  // --------------------------------------------------------------------------

  describe("signed submission (SecurityLevel Sign)", () => {
    const HOST = "it-collectd-signed";
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: HOST },
        "/settings/collectd/client/targets/default": {
          address: `127.0.0.1:${signPort}`,
          "security level": "sign",
          user: AUTH_USER,
          password: AUTH_PASSWORD,
        },
      });
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("stores value-lists the server verified against the AuthFile", async () => {
      const files = await waitForFiles(HOST, (f) => f.some((p) => /\/cpu-total\/cpu-user-/.test(p)));
      expect(files.some((p) => /\/cpu-total\/cpu-user-/.test(p))).toBe(true);
    });
  });

  describe("encrypted submission (SecurityLevel Encrypt)", () => {
    const HOST = "it-collectd-encrypted";
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: HOST },
        "/settings/collectd/client/targets/default": {
          address: `127.0.0.1:${encryptPort}`,
          "security level": "encrypt",
          user: AUTH_USER,
          password: AUTH_PASSWORD,
        },
      });
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("stores value-lists the server decrypted and checksum-verified", async () => {
      const files = await waitForFiles(HOST, (f) => f.some((p) => /\/cpu-total\/cpu-user-/.test(p)));
      expect(files.some((p) => /\/cpu-total\/cpu-user-/.test(p))).toBe(true);
    });
  });

  describe("unsigned traffic to the Sign listener", () => {
    const REJECT_HOST = "it-collectd-rejected";
    const CONTROL_HOST = "it-collectd-control";
    let rejected: NscpInstance;
    let control: NscpInstance;

    beforeAll(async () => {
      // One plaintext sender aimed at the Sign listener (must be dropped) and
      // one aimed at the None listener (the timing control: once its metrics
      // are stored, the rejected sender has been transmitting just as long).
      rejected = new NscpInstance();
      await rejected.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: REJECT_HOST },
        "/settings/collectd/client/targets/default": { address: `127.0.0.1:${signPort}` },
      });
      control = new NscpInstance();
      await control.configure({
        "/modules": { CheckSystem: "enabled", CollectdClient: "enabled" },
        "/settings/core": { "metrics interval": "1s" },
        "/settings/collectd/client": { hostname: CONTROL_HOST },
        "/settings/collectd/client/targets/default": { address: `127.0.0.1:${port}` },
      });
      rejected.start();
      control.start();
    });

    afterAll(async () => {
      await rejected?.stop();
      await control?.stop();
    });

    it("is rejected while plaintext to the None listener still lands", async () => {
      // Wait until the control sender's metrics are stored, then a little
      // longer, so the rejected sender has provably had time to be heard.
      const controlFiles = await waitForFiles(CONTROL_HOST, (f) => f.some((p) => /\/cpu-total\/cpu-user-/.test(p)));
      expect(controlFiles.some((p) => /\/cpu-total\/cpu-user-/.test(p))).toBe(true);
      await new Promise((r) => setTimeout(r, 3000));
      expect(await dataFiles(REJECT_HOST)).toEqual([]);
    });
  });
});
