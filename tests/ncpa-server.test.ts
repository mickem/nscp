/**
 * The NCPA server (HTTPS, port 5693) over its own wire, without a
 * check_ncpa client: curl against the endpoint, asserting the JSON
 * shapes and the auth/authorisation contract.
 *
 * Needs no docker, so this runs everywhere the rest-* suites do. The
 * companion ncpa-client.test.ts drives the same server with the stock
 * check_ncpa.py from a container.
 *
 * What is load-bearing here:
 *   - `check=1` is a valued boolean over REST — the trap that
 *     `po::bool_switch` falls into, except this server parses the query
 *     string itself, which is exactly why the case is pinned.
 *   - An error is HTTP 200 with a JSON `error` key, never a 4xx:
 *     check_ncpa.py turns any non-2xx into "UNKNOWN: An error occurred
 *     connecting to API", which tells an operator nothing.
 *   - `allowed hosts` and a bad token are two different refusals, and
 *     neither echoes the token.
 */
import * as fs from "fs";
import * as path from "path";
import execa from "execa";
import { NscpInstance, generateCertChain } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";
const PORT = 5693;
const TOKEN = "an-ncpa-integration-token";
const BASE = `https://127.0.0.1:${PORT}/api`;

/** GET a URL with curl, returning the body whatever the status code is. */
async function get(url: string): Promise<string> {
  const r = await execa("curl", ["-s", "-k", url], { timeout: 30_000, reject: false });
  return r.stdout;
}

/** GET a URL, returning only the HTTP status code. */
async function status(url: string): Promise<string> {
  const r = await execa("curl", ["-s", "-k", "-o", "/dev/null", "-w", "%{http_code}", url], {
    timeout: 30_000,
    reject: false,
  });
  return r.stdout.trim();
}

async function getJson(url: string): Promise<any> {
  const body = await get(url);
  try {
    return JSON.parse(body);
  } catch (e) {
    throw new Error(`Not JSON from ${url}: ${body}`);
  }
}

function api(node: string, params: Record<string, string> = {}): string {
  const query = new URLSearchParams({ token: TOKEN, ...params }).toString();
  return `${BASE}${node ? "/" + node : ""}?${query}`;
}

/**
 * Wait until the core has handed the module a metrics snapshot. Everything
 * under cpu / memory / disk / interface is rendered from it, and until the
 * first one arrives those nodes are empty - which a check reports as "the node
 * does not exist", not as an error. The agent is restarted several times in
 * this suite and each restart starts from no snapshot, so this is a wait the
 * snapshot-backed tests do for themselves rather than once in beforeAll.
 */
async function waitForSnapshot(timeoutMs = 60_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const body = await getJson(api("memory/virtual/total"));
    if (Array.isArray(body.total)) return;
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  throw new Error("No metrics snapshot within the timeout");
}

describe("NCPA server", () => {
  let nscp: NscpInstance;
  let helloScript: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // An external script, so `plugins = scripts` has a command that is one.
    helloScript = path.join(nscp.scratch("scripts"), onWindows ? "hello.bat" : "hello.sh");
    fs.writeFileSync(
      helloScript,
      onWindows
        ? "@echo OK: hello from an external script^|'x'=1\r\n@exit /b 0\r\n"
        : "#!/bin/sh\necho \"OK: hello from an external script|'x'=1\"\nexit 0\n",
    );
    fs.chmodSync(helloScript, 0o755);
    const certs = generateCertChain({
      outDir: nscp.scratch("certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    await nscp.configure({
      "/modules": {
        NCPAServer: "enabled",
        // The built-in nodes are rendered from the 1 Hz metrics snapshot
        // these two publish; CheckHelpers gives `plugins/` something
        // deterministic to run, and CheckExternalScripts something that is a
        // script rather than a built-in, which is what `plugins = scripts`
        // has to tell apart.
        CheckSystem: "enabled",
        CheckDisk: "enabled",
        CheckHelpers: "enabled",
        CheckExternalScripts: "enabled",
      },
      "/settings/external scripts/scripts": { check_hello: helloScript },
      // The built-in nodes are rendered from the metrics snapshot the core
      // hands consumers every `metrics interval`, which defaults to 10s.
      // Shorten it so a suite does not spend that long waiting for its first
      // sample.
      "/settings/core": { "metrics interval": "1s" },
      "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
      "/settings/NCPA/server": {
        token: TOKEN,
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
        // The suite intentionally probes failed auth; leave the limiter
        // out of it so a later test is not blocked by an earlier one.
        "auth rate limit max failures": "0",
      },
    });
    await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
    await waitForSnapshot();
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("authentication", () => {
    it("answers the NCPA error body, with HTTP 200, for a wrong token", async () => {
      const url = `${BASE}/cpu?token=not-the-token`;
      expect(await status(url)).toBe("200");
      const body = await getJson(url);
      expect(body.error).toBe("Incorrect credentials given.");
    });

    it("answers the same error when no token is sent at all", async () => {
      const body = await getJson(`${BASE}/cpu`);
      expect(body.error).toBe("Incorrect credentials given.");
    });

    it("never echoes the token in the refusal", async () => {
      const body = await get(`${BASE}/cpu?token=nearly-${TOKEN}`);
      expect(body).not.toContain(TOKEN);
    });

    it("accepts the backup token as well as the primary one", async () => {
      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { "backup token": "the-rotation-token" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });

      expect(
        (await getJson(`${BASE}/system/agent_version?token=the-rotation-token`)).error,
      ).toBeUndefined();
      expect((await getJson(api("system/agent_version"))).error).toBeUndefined();
    });
  });

  describe("list mode", () => {
    it("answers the whole tree under a `root` key", async () => {
      const body = await getJson(api(""));
      expect(Object.keys(body)).toEqual(["root"]);
      // The node set the Nagios XI wizard walks.
      for (const node of [
        "cpu",
        "memory",
        "disk",
        "interface",
        "system",
        "user",
        "plugins",
        "services",
        "processes",
      ]) {
        expect(body.root).toHaveProperty(node);
      }
    });

    it("renders a scalar leaf as [value, unit] and a per-core leaf as [[values], unit]", async () => {
      await waitForSnapshot();
      const total = await getJson(api("memory/virtual/total"));
      expect(Array.isArray(total.total)).toBe(true);
      expect(typeof total.total[0]).toBe("number");
      expect(total.total[1]).toBe("B");

      const count = await getJson(api("cpu/count"));
      // cpu/count is natively a list, so it stays one even on a one-core box.
      expect(Array.isArray(count.count[0])).toBe(true);
      expect(count.count[1]).toBe("cores");
    });

    it("scales a byte node with `units` and leaves a percentage alone", async () => {
      await waitForSnapshot();
      const scaled = await getJson(api("memory/virtual/total", { units: "Gi" }));
      expect(scaled.total[1]).toBe("GiB");
      const percent = await getJson(api("memory/virtual/percent", { units: "Gi" }));
      expect(percent.percent[1]).toBe("%");
    });

    it("reports a missing node as an NCPA error node rather than a 404", async () => {
      const url = api("cpu/there-is-no-such-node");
      expect(await status(url)).toBe("200");
      const body = await getJson(url);
      expect(body.error.code).toBe(100);
      expect(body.error.node).toBe("there-is-no-such-node");
    });
  });

  describe("check mode", () => {
    it("accepts `check=1` as a valued boolean and answers returncode + stdout", async () => {
      await waitForSnapshot();
      const body = await getJson(
        api("memory/virtual", { check: "1", warning: "99", critical: "100" }),
      );
      expect(body.returncode).toBe(0);
      expect(body.stdout).toMatch(/^OK: Memory usage was /);
      // The parent folds its children into the line and appends perfdata.
      expect(body.stdout).toContain("Total:");
      expect(body.stdout).toContain("'percent'=");
    });

    it("applies the thresholds", async () => {
      await waitForSnapshot();
      const body = await getJson(
        api("memory/virtual", { check: "1", warning: "0", critical: "100" }),
      );
      expect(body.returncode).toBe(1);
      expect(body.stdout).toMatch(/^WARNING: /);
    });

    it("reports a malformed threshold as UNKNOWN instead of a silent OK", async () => {
      await waitForSnapshot();
      const body = await getJson(api("memory/virtual/percent", { check: "1", warning: "eighty" }));
      expect(body.returncode).toBe(3);
    });

    it("treats `delta=False` as off, the way the switch reads", async () => {
      await waitForSnapshot();
      // check_ncpa.py only sends `delta` when -d was given, so the server
      // never sees this from the stock plugin - but a hand-written URL that
      // says False must not mean True, which is what the real agent's Python
      // truthiness would make of it.
      const off = await getJson(api("system/uptime", { check: "1", delta: "False" }));
      expect(off.returncode).toBe(0);
      expect(off.stdout).not.toContain("/s");
    });

    it("`delta` renames the unit of a value the collector already samples per second", async () => {
      await waitForSnapshot();
      const interfaces = await getJson(api("interface"));
      const names = Object.keys(interfaces.interface);
      expect(names.length).toBeGreaterThan(0);
      const node = `interface/${encodeURIComponent(names[0])}/bytes_sent`;

      const plain = await getJson(api(node));
      expect(plain.bytes_sent[1]).toBe("B");
      const delta = await getJson(api(node, { delta: "1" }));
      // The number is already a rate, so only the unit changes - differencing
      // two rates would report ~0 for a busy interface.
      expect(delta.bytes_sent[1]).toBe("B/s");
    });

    it("checks a mount point addressed by its pipe-encoded name", async () => {
      await waitForSnapshot();
      const logical = await getJson(api("disk/logical"));
      const mounts = Object.keys(logical.logical);
      expect(mounts.length).toBeGreaterThan(0);
      const body = await getJson(
        api(`disk/logical/${encodeURIComponent(mounts[0])}`, {
          check: "1",
          warning: "100",
          critical: "100",
        }),
      );
      expect(body.returncode).toBe(0);
      expect(body.stdout).toMatch(/^OK: Used disk space was /);
    });

    it("checks a service by name", async () => {
      const services = await getJson(api("services"));
      const names = Object.keys(services.services);
      if (names.length === 0) {
        // A container without an init system reports no services. The
        // documented no-data contract is UNKNOWN with a message saying so,
        // not an empty OK.
        const body = await getJson(api("services", { check: "1", service: "anything" }));
        expect(body.returncode).toBe(3);
        expect(body.stdout).toContain("No services found");
        return;
      }
      const body = await getJson(api("services", { check: "1", service: names[0] }));
      expect(body.stdout).toContain(names[0]);
    });

    it("counts processes and names what it counted", async () => {
      const body = await getJson(api("processes", { check: "1", name: "nscp" }));
      expect(body.stdout).toContain("Process count for processes named nscp");
      expect(body.stdout).toContain("'process_count'=");
    });
  });

  describe("the plugins node", () => {
    it("lists the exposed queries", async () => {
      const body = await getJson(api("plugins"));
      expect(Array.isArray(body.plugins)).toBe(true);
      expect(body.plugins).toContain("check_uptime");
    });

    it("runs a query and hands back its return code and output unchanged", async () => {
      // CheckHelpers' check_ok / check_critical are the deterministic pair:
      // they say exactly what they were told to and exit 0 / 2 whatever the
      // state of the host, so this asserts the passthrough rather than the
      // weather.
      const ok = await getJson(api("plugins/check_ok"));
      expect(ok.returncode).toBe(0);
      const critical = await getJson(api("plugins/check_critical"));
      expect(critical.returncode).toBe(2);
    });

    it("answers the same document with and without check=1", async () => {
      // A plugin's document is the whole body, not the value of a
      // {"<name>": ...} pair — check_ncpa.py reads `stdout` and `returncode`
      // at the top level.
      const walked = await getJson(api("plugins/check_critical"));
      const checked = await getJson(api("plugins/check_critical", { check: "1" }));
      expect(walked).toEqual(checked);
    });

    it("refuses path arguments while `allow arguments` is off", async () => {
      const body = await getJson(api("plugins/check_ok/message%3Dhello"));
      expect(body.returncode).toBe(3);
      expect(body.stdout).toContain("Arguments are not allowed");
    });

    it("accepts path arguments once `allow arguments` is on", async () => {
      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { "allow arguments": "true" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });

      // One path segment, percent-encoded exactly as check_ncpa.py quotes
      // each -a token, arriving as one REST-style key=value token — so the
      // message coming back is what proves the argument reached the check.
      const body = await getJson(api("plugins/check_ok/message%3Dhello-from-ncpa"));
      expect(body.returncode).toBe(0);
      expect(body.stdout).toBe("hello-from-ncpa");
    });

    it("exposes only external scripts with `plugins = scripts`", async () => {
      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { plugins: "scripts" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });

      // Which commands are scripts is asked of the core's registry, not
      // guessed from the name - `check_hello` is a script and `check_ok` is a
      // built-in, and both are spelled the same way.
      expect((await getJson(api("plugins"))).plugins).toEqual(["check_hello"]);
      const script = await getJson(api("plugins/check_hello", { check: "1" }));
      expect(script.returncode).toBe(0);
      expect(script.stdout).toContain("hello from an external script");
      expect((await getJson(api("plugins/check_ok", { check: "1" }))).returncode).toBe(3);

      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { plugins: "any" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
    });

    it("hides everything outside an explicit allow-list", async () => {
      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { plugins: "check_ok" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });

      expect((await getJson(api("plugins"))).plugins).toEqual(["check_ok"]);
      expect((await getJson(api("plugins/check_ok"))).returncode).toBe(0);

      // A query outside the list is not there at all, so it answers like any
      // other missing node: the error object to a walk, UNKNOWN to a check -
      // and check_ncpa.py always sends check=1, so that is what a caller sees.
      const walked = await getJson(api("plugins/check_cpu"));
      expect(walked.error.plugin).toBe("check_cpu");
      const hidden = await getJson(api("plugins/check_cpu", { check: "1" }));
      expect(hidden.returncode).toBe(3);
      expect(hidden.stdout).toContain("does not exist");

      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { plugins: "any" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
    });

    it("honours `sleep` and accepts the same parameters in a POST body", async () => {
      await waitForSnapshot();
      const started = Date.now();
      await getJson(api("cpu/count", { sleep: "2" }));
      expect(Date.now() - started).toBeGreaterThanOrEqual(1500);

      // The agent accepts POST as well as GET, with the parameters
      // form-encoded in the body.
      const r = await execa(
        "curl",
        [
          "-s",
          "-k",
          "-X",
          "POST",
          "-d",
          `token=${TOKEN}&check=1&warning=100`,
          `${BASE}/memory/virtual`,
        ],
        { timeout: 30_000, reject: false },
      );
      expect(JSON.parse(r.stdout).stdout).toMatch(/^OK: Memory usage was /);
    });
  });

  describe("refusals that are not check results", () => {
    it("answers HTTP 403 for an address outside `allowed hosts`", async () => {
      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { "allowed hosts": "10.11.12.13" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });

      // A network-policy refusal, not a CRITICAL check: the plugin must
      // report that it could not reach the API, not that the host is sick.
      expect(await status(api("cpu"))).toBe("403");

      await nscp.stop();
      await nscp.configure({ "/settings/NCPA/server": { "allowed hosts": "127.0.0.1,::1" } });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
      expect(await status(api("cpu"))).toBe("200");
    });

    it("refuses to start in cleartext when the certificate is missing", async () => {
      await nscp.stop();
      await nscp.configure({
        "/settings/NCPA/server": {
          certificate: path.join(nscp.workDir, "there-is-no-such-certificate.pem"),
          "certificate key": "",
        },
      });
      nscp.start();
      // The module logs the refusal and the port never opens, rather than
      // silently serving the token in clear.
      await expect(nscp.waitForPort(PORT, { timeoutMs: 8_000 })).rejects.toThrow();
    });
  });
});
