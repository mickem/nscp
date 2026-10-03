/**
 * NCPAServer: the Nagios NCPA HTTP API on port 5693, driven over HTTPS the way
 * check_ncpa.py drives it (GET /api/<node path>/<arg>/<arg>?token=...&check=1),
 * without Docker. Covers the token gate (primary, backup, wrong, missing, none
 * configured), the `plugins/` node (listing, dispatch, argument handling and
 * the `allow arguments` / `plugins` policies), the NCPA error shapes, a POST
 * form body, `allowed hosts`, and the refusal to serve the token in clear when
 * there is no certificate.
 *
 * NCPA answers authentication and lookup failures with HTTP 200 and a JSON
 * body (check_ncpa.py turns `{"error": ...}` into CRITICAL). A host outside
 * `allowed hosts` gets no answer at all: its connection is dropped as it is
 * accepted, before the TLS handshake.
 */
import * as fs from "fs";
import * as net from "net";
import * as os from "os";
import * as path from "path";
import request from "supertest";
import { NscpInstance, onWindows } from "@fixtures/index";

jest.setTimeout(900_000);

const PORT = 5693;
const URL = `https://127.0.0.1:${PORT}`;
const TOKEN = "ncpa-primary-token";
const BACKUP = "ncpa-backup-token";
// On Windows the external scripts are .bat files run as `cmd /c <path>`, as
// in the other suites: a command line with no path in it is started with
// CreateProcess("cmd ..."), which does not search PATH for `cmd`.
const winScripts = onWindows ? fs.mkdtempSync(path.join(os.tmpdir(), "nscp-ncpa-")) : "";
function winScript(name: string, body: string): string {
  const file = path.join(winScripts, `${name}.bat`);
  fs.writeFileSync(file, `@echo off\r\n${body}\r\n`);
  return `cmd /c ${file}`;
}
const SCRIPT = "ncpa_echo";
const SCRIPT_COMMAND = onWindows
  ? winScript(SCRIPT, "echo script-output")
  : "/bin/echo script-output";
// Prints its first two arguments, to see what order they arrive in.
const ARGS_SCRIPT = "ncpa_args";
const ARGS_COMMAND = onWindows
  ? `${winScript(ARGS_SCRIPT, "echo first=%1 second=%2")} $ARG1$ $ARG2$`
  : "/bin/echo first=$ARG1$ second=$ARG2$";
// Takes a few seconds, like an external script near its timeout.
const SLOW_SCRIPT = "ncpa_slow";
const SLOW_COMMAND = onWindows
  ? winScript(SLOW_SCRIPT, "ping -n 4 127.0.0.1 >nul")
  : "/bin/sleep 3";
const ALIAS = "ncpa_alias";

type Settings = Record<string, Record<string, string | number | boolean>>;

/** Configure from scratch and start an agent serving NCPA; resolves once it listens. */
async function startNcpa(
  server: Record<string, string | number | boolean>,
  extra: Settings = {},
  // Other ports the agent will bind, waited free first (the REST suites share 8443).
  alsoBinds: number[] = [],
): Promise<NscpInstance> {
  const nscp = new NscpInstance();
  await nscp.configure({
    "/modules": { NCPAServer: "enabled", CheckHelpers: "enabled", CheckExternalScripts: "enabled" },
    "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
    "/settings/external scripts": { "allow arguments": true },
    "/settings/external scripts/scripts": {
      [SCRIPT]: SCRIPT_COMMAND,
      [ARGS_SCRIPT]: ARGS_COMMAND,
      [SLOW_SCRIPT]: SLOW_COMMAND,
    },
    // A query alias: the registry lists these apart from the queries.
    "/settings/check helpers/alias": { [ALIAS]: "check_ok message=via-alias" },
    // The suite probes wrong tokens on purpose.
    "/settings/NCPA/server": { "auth rate limit max failures": 0, ...server },
    ...extra,
  });
  for (const port of [PORT, ...alsoBinds]) await nscp.waitForPortFree(port, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
  return nscp;
}

const get = (target: string) => request(URL).get(target).trustLocalhost(true);

/** Poll the agent's output until `text` shows up (or give up after 30 s). */
async function waitForLog(nscp: NscpInstance, text: string): Promise<void> {
  const deadline = Date.now() + 30_000;
  while (Date.now() < deadline && !nscp.capturedStdout().includes(text)) {
    await new Promise((r) => setTimeout(r, 250));
  }
}

describe("NCPA server", () => {
  describe("with a token and the defaults", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({ token: TOKEN, "backup token": BACKUP });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("refuses a wrong token with the NCPA error body", async () => {
      const res = await get("/api/plugins?token=wrong").expect(200);
      expect(res.body).toEqual({ error: "Incorrect credentials given." });
    });

    it("refuses a missing token the same way", async () => {
      const res = await get("/api/plugins/check_ok?check=1").expect(200);
      expect(res.body).toEqual({ error: "Incorrect credentials given." });
    });

    it("never echoes the token back", async () => {
      const res = await get(`/api/plugins/no_such_plugin?token=${TOKEN}`).expect(200);
      expect(res.text).not.toContain(TOKEN);
    });

    it("lists the registered queries under plugins", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(res.headers["content-type"]).toContain("application/json");
      expect(Array.isArray(res.body.plugins)).toBe(true);
      expect(res.body.plugins).toEqual(
        expect.arrayContaining(["check_ok", "check_critical", SCRIPT]),
      );
      expect([...res.body.plugins].sort()).toEqual(res.body.plugins);
    });

    it("lists the plugins node at the root too", async () => {
      const res = await get(`/api?token=${TOKEN}`).expect(200);
      expect(res.body.root.plugins).toEqual(expect.arrayContaining(["check_ok"]));
    });

    it("runs a check in check mode (the trailing slash check_ncpa adds is ignored)", async () => {
      const res = await get(
        `/api/plugins/check_critical/?token=${TOKEN}&check=1&delta=False&units=G`,
      ).expect(200);
      expect(res.body.returncode).toBe(2);
      expect(typeof res.body.stdout).toBe("string");
    });

    it("runs a check without check=1 as well, as NCPA does for plugins", async () => {
      const res = await get(`/api/plugins/check_ok?token=${TOKEN}`).expect(200);
      expect(res.body.returncode).toBe(0);
    });

    it("accepts check=true as a valued boolean", async () => {
      const res = await get(`/api/plugins/check_ok?token=${TOKEN}&check=true`).expect(200);
      expect(res.body.returncode).toBe(0);
    });

    it("accepts the backup token", async () => {
      const res = await get(`/api/plugins/check_ok?token=${BACKUP}&check=1`).expect(200);
      expect(res.body.returncode).toBe(0);
    });

    it("refuses path arguments while allow arguments is false", async () => {
      const res = await get(`/api/plugins/check_warning/hello?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.returncode).toBe(3);
      expect(res.body.stdout).toContain("Arguments are not allowed");
    });

    it("refuses args= parameters while allow arguments is false", async () => {
      const res = await get(`/api/plugins/check_warning?token=${TOKEN}&check=1&args=hello`).expect(
        200,
      );
      expect(res.body.returncode).toBe(3);
      expect(res.body.stdout).toContain("Arguments are not allowed");
    });

    it("answers an unknown plugin like NCPA, in list and in check mode", async () => {
      const walk = await get(`/api/plugins/no_such_plugin?token=${TOKEN}`).expect(200);
      expect(walk.body.error).toMatchObject({
        code: 100,
        plugin: "no_such_plugin",
        message: "The plugin requested does not exist.",
        path: "/api/plugins/no_such_plugin",
      });
      const check = await get(`/api/plugins/no_such_plugin?token=${TOKEN}&check=1`).expect(200);
      expect(check.body).toEqual({
        returncode: 3,
        stdout: "UNKNOWN: The plugin (no_such_plugin) requested does not exist.",
      });
    });

    it("answers a node that is not served yet as a missing node", async () => {
      const res = await get(`/api/cpu/percent?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.returncode).toBe(3);
      expect(res.body.stdout).toContain("The node (cpu) requested does not exist.");
    });

    it("takes the token and parameters from a POST form body", async () => {
      const res = await request(URL)
        .post("/api/plugins/check_critical")
        .trustLocalhost(true)
        .type("form")
        .send({ token: TOKEN, check: "1" })
        .expect(200);
      expect(res.body.returncode).toBe(2);
    });

    it("runs an external script", async () => {
      const res = await get(`/api/plugins/${SCRIPT}?token=${TOKEN}&check=1`).expect(200);
      expect(res.body).toEqual({ returncode: 0, stdout: "script-output" });
    });

    it("lists and runs query aliases", async () => {
      const list = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(list.body.plugins).toContain(ALIAS);
      const res = await get(`/api/plugins/${ALIAS}?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.returncode).toBe(0);
      expect(res.body.stdout).toContain("via-alias");
    });

    it("keeps answering while a slow check runs", async () => {
      // Checks used to run on the listener's only thread: one slow external
      // script stalled every other poll on the port.
      const slow = get(`/api/plugins/${SLOW_SCRIPT}?token=${TOKEN}&check=1`).then((r) => r);
      await new Promise((r) => setTimeout(r, 300));
      const started = Date.now();
      const fast = await get(`/api/plugins/check_ok?token=${TOKEN}&check=1`).expect(200);
      const elapsed = Date.now() - started;
      expect(fast.body.returncode).toBe(0);
      expect(elapsed).toBeLessThan(1500);
      expect((await slow).body.returncode).toBe(0);
    });

    it("does not answer outside /api", async () => {
      await get(`/apix?token=${TOKEN}`).expect(404);
    });
  });

  describe("with allow arguments and a plugins list", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({
        token: TOKEN,
        "allow arguments": true,
        plugins: `check_warning, CHECK_OK, ${ARGS_SCRIPT}`,
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("lists only the exposed queries", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(res.body).toEqual({ plugins: ["check_ok", "check_warning", ARGS_SCRIPT] });
    });

    it("re-splits quoted path segments the way NCPA does", async () => {
      // What check_ncpa.py sends for -a '"message=hello world"': its non-POSIX
      // shlex keeps the quotes in the segment, and the server's second split
      // removes them.
      const res = await get(
        `/api/plugins/check_warning/%22message%3Dhello%20world%22?token=${TOKEN}&check=1`,
      ).expect(200);
      expect(res.body.returncode).toBe(1);
      expect(res.body.stdout).toContain("hello world");
    });

    it("splits an args= value like a shell", async () => {
      const res = await get(
        `/api/plugins/check_warning?token=${TOKEN}&check=1&args=${encodeURIComponent('"message=from args"')}`,
      ).expect(200);
      expect(res.body.returncode).toBe(1);
      expect(res.body.stdout).toContain("from args");
    });

    it("puts args= values before the path segments, as NCPA does", async () => {
      const res = await get(
        `/api/plugins/${ARGS_SCRIPT}/from-path?token=${TOKEN}&check=1&args=from-args`,
      ).expect(200);
      expect(res.body.returncode).toBe(0);
      expect(res.body.stdout).toBe("first=from-args second=from-path");
    });

    it("keeps backslashes in an argument", async () => {
      // A Windows path: NCPA on Windows splits with posix=False and keeps them.
      const arg = encodeURIComponent("message=C:\\Windows\\Temp");
      const res = await get(`/api/plugins/check_ok/${arg}?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.stdout).toContain("C:\\Windows\\Temp");
    });

    it("keeps a filter's own single quotes", async () => {
      const arg = encodeURIComponent("message=core='total'");
      const res = await get(`/api/plugins/check_ok/${arg}?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.stdout).toContain("core='total'");
    });

    it("answers a registered but unlisted query as missing", async () => {
      const res = await get(`/api/plugins/check_critical?token=${TOKEN}&check=1`).expect(200);
      expect(res.body).toEqual({
        returncode: 3,
        stdout: "UNKNOWN: The plugin (check_critical) requested does not exist.",
      });
    });
  });

  describe("with plugins = scripts", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({ token: TOKEN, plugins: "scripts" });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("exposes the external scripts and nothing else", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(res.body.plugins).toContain(SCRIPT);
      expect(res.body.plugins).not.toContain("check_ok");
      const check = await get(`/api/plugins/check_ok?token=${TOKEN}&check=1`).expect(200);
      expect(check.body.returncode).toBe(3);
    });
  });

  describe("with plugins = scripts and CheckExternalScripts loaded under an alias", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      // The registry names a query's owner by the module's alias; the policy
      // has to see through it.
      nscp = await startNcpa(
        { token: TOKEN, plugins: "scripts" },
        {
          "/modules": {
            NCPAServer: "enabled",
            CheckHelpers: "enabled",
            myscripts: "CheckExternalScripts",
          },
          "/settings/myscripts/scripts": { [SCRIPT]: SCRIPT_COMMAND },
        },
      );
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("still exposes the external scripts", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(res.body.plugins).toContain(SCRIPT);
      expect(res.body.plugins).not.toContain("check_ok");
      const check = await get(`/api/plugins/${SCRIPT}?token=${TOKEN}&check=1`).expect(200);
      expect(check.body).toEqual({ returncode: 0, stdout: "script-output" });
    });
  });

  describe("with the token rate limit on", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({
        token: TOKEN,
        "auth rate limit max failures": 3,
        "auth rate limit block seconds": 60,
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("counts parallel guesses one at a time", async () => {
      // Answered by several workers at once, guesses used to all pass the block
      // check before any of them was counted.
      const answers = await Promise.all(
        Array.from({ length: 10 }, (_, i) => get(`/api/plugins/check_ok?token=wrong-${i}&check=1`)),
      );
      const errors = answers.map((r) => r.body.error);
      expect(errors.filter((e) => e === "Incorrect credentials given.")).toHaveLength(3);
      expect(
        errors.filter((e) => e === "Too many failed authentication attempts, try again later."),
      ).toHaveLength(7);
      // Blocked means blocked: the right token is refused too, for now.
      const res = await get(`/api/plugins/check_ok?token=${TOKEN}&check=1`).expect(200);
      expect(res.body.error).toBe("Too many failed authentication attempts, try again later.");
    });
  });

  describe("without a token", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({});
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("refuses every request, whatever token is sent", async () => {
      for (const token of ["", "mytoken", TOKEN]) {
        const res = await get(`/api/plugins/check_ok?token=${token}&check=1`).expect(200);
        expect(res.body).toEqual({ error: "Incorrect credentials given." });
      }
      expect(nscp.capturedStdout()).toContain("no token is configured");
    });
  });

  describe("from a host outside allowed hosts", () => {
    let nscp: NscpInstance;

    beforeAll(async () => {
      nscp = await startNcpa({ token: TOKEN, "allowed hosts": "192.0.2.10" });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("drops the connection before the TLS handshake, whatever the request", async () => {
      // Checked as the connection is accepted, so a refused host gets no
      // handshake, no worker and no answer - not even to a verb no route
      // handles.
      for (const verb of ["get", "put", "head"] as const) {
        await expect(
          request(URL)[verb](`/api/plugins?token=${TOKEN}`).trustLocalhost(true),
        ).rejects.toThrow();
      }
      expect(nscp.capturedStdout()).toContain("not in 'allowed hosts'");
    });
  });

  describe("without a certificate", () => {
    let nscp: NscpInstance | undefined;

    afterAll(async () => {
      await nscp?.stop();
    });

    it("refuses to serve the token in clear", async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { NCPAServer: "enabled" },
        "/settings/default": { "allowed hosts": "127.0.0.1" },
        // Not named certificate.pem, so no default one is generated in its place.
        "/settings/NCPA/server": {
          token: TOKEN,
          certificate: "${certificate-path}/no-such-cert.crt",
        },
      });
      await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
      nscp.start();
      await waitForLog(nscp, "NCPA listener has NOT been started");
      expect(nscp.capturedStdout()).toContain(
        "refusing to start the NCPA listener in cleartext HTTP",
      );
      await expect(nscp.waitForPort(PORT, { timeoutMs: 2_000 })).rejects.toThrow();
    });
  });

  describe("alongside the WEB server", () => {
    let nscp: NscpInstance;
    const WEB_PORT = 8443;

    beforeAll(async () => {
      // Two HTTP servers in one agent: each has its own listener, its own
      // credentials and (on the mongoose backend) its own log routing.
      nscp = await startNcpa(
        { token: TOKEN },
        {
          "/modules": { NCPAServer: "enabled", CheckHelpers: "enabled", WEBServer: "enabled" },
          "/settings/WEB/server/users/admin": { role: "full", password: "web-password" },
        },
        [WEB_PORT],
      );
      await nscp.waitForPort(WEB_PORT, { timeoutMs: 30_000 });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("both answer, each with its own credentials", async () => {
      const ncpa = get(`/api/plugins/check_ok?token=${TOKEN}&check=1`);
      const web = request(`https://127.0.0.1:${WEB_PORT}`)
        .get("/api/v2/info")
        .auth("admin", "web-password")
        .trustLocalhost(true);
      const [n, w] = await Promise.all([ncpa, web]);
      expect(n.body.returncode).toBe(0);
      expect(w.status).toBe(200);
      // The NCPA token opens nothing on the WEB server.
      await request(`https://127.0.0.1:${WEB_PORT}`)
        .get(`/api/v2/info?token=${TOKEN}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });

  describe("with a certificate that does not load", () => {
    let nscp: NscpInstance | undefined;

    afterAll(async () => {
      await nscp?.stop();
    });

    it("refuses to fall back to cleartext", async () => {
      // The file exists, so the old check (does the file exist?) passed, the
      // certificate then failed to load and the listener served plain HTTP.
      nscp = new NscpInstance();
      const junk = path.join(nscp.workDir, "junk.pem");
      fs.writeFileSync(junk, "this is not a certificate\n");
      await nscp.configure({
        "/modules": { NCPAServer: "enabled" },
        "/settings/default": { "allowed hosts": "127.0.0.1" },
        "/settings/NCPA/server": { token: TOKEN, certificate: junk },
      });
      await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
      nscp.start();
      await waitForLog(nscp, "NCPA listener has NOT been started");
      expect(nscp.capturedStdout()).toContain(
        "could not be loaded: refusing to start the NCPA listener in cleartext HTTP",
      );
      await expect(nscp.waitForPort(PORT, { timeoutMs: 2_000 })).rejects.toThrow();
    });
  });

  describe("with a certificate that does not load and allow insecure", () => {
    let nscp: NscpInstance | undefined;

    afterAll(async () => {
      await nscp?.stop();
    });

    it("serves plain HTTP, because it was asked to", async () => {
      // A failed setSsl() makes a server refuse to start; the explicit opt-in
      // gets a server that was never asked for TLS.
      nscp = new NscpInstance();
      const junk = path.join(nscp.workDir, "junk.pem");
      fs.writeFileSync(junk, "this is not a certificate\n");
      await nscp.configure({
        "/modules": { NCPAServer: "enabled", CheckHelpers: "enabled" },
        "/settings/default": { "allowed hosts": "127.0.0.1" },
        "/settings/NCPA/server": { token: TOKEN, certificate: junk, "allow insecure": true },
      });
      await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
      nscp.start();
      await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
      const res = await request(`http://127.0.0.1:${PORT}`)
        .get(`/api/plugins/check_ok?token=${TOKEN}&check=1`)
        .expect(200);
      expect(res.body.returncode).toBe(0);
    });
  });

  describe("when the port is taken", () => {
    let nscp: NscpInstance | undefined;
    let squatter: net.Server | undefined;

    afterAll(async () => {
      await nscp?.stop();
      await new Promise<void>((r) => (squatter ? squatter.close(() => r()) : r()));
    });

    it("says the listener did not start", async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { NCPAServer: "enabled" },
        "/settings/default": { "allowed hosts": "127.0.0.1" },
        "/settings/NCPA/server": { token: TOKEN },
      });
      await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
      squatter = net.createServer();
      await new Promise<void>((r) => squatter!.listen(PORT, "0.0.0.0", () => r()));
      nscp.start();
      // It used to log "listening on port 5693" here.
      await waitForLog(nscp, "NCPA listener has NOT been started on port");
      expect(nscp.capturedStdout()).not.toContain("NCPA: listening on port");
    });
  });
});
