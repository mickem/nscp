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
 * body (check_ncpa.py turns `{"error": ...}` into CRITICAL); only a host
 * outside `allowed hosts` gets a bare HTTP 403.
 */
import request from "supertest";
import { NscpInstance, onWindows } from "@fixtures/index";

jest.setTimeout(900_000);

const PORT = 5693;
const URL = `https://127.0.0.1:${PORT}`;
const TOKEN = "ncpa-primary-token";
const BACKUP = "ncpa-backup-token";
const SCRIPT = "ncpa_echo";
const SCRIPT_COMMAND = onWindows ? "cmd /c echo script-output" : "/bin/echo script-output";

type Settings = Record<string, Record<string, string | number | boolean>>;

/** Configure from scratch and start an agent serving NCPA; resolves once it listens. */
async function startNcpa(
  server: Record<string, string | number | boolean>,
  extra: Settings = {},
): Promise<NscpInstance> {
  const nscp = new NscpInstance();
  await nscp.configure({
    "/modules": { NCPAServer: "enabled", CheckHelpers: "enabled", CheckExternalScripts: "enabled" },
    "/settings/default": { "allowed hosts": "127.0.0.1,::1" },
    "/settings/external scripts/scripts": { [SCRIPT]: SCRIPT_COMMAND },
    // The suite probes wrong tokens on purpose.
    "/settings/NCPA/server": { "auth rate limit max failures": 0, ...server },
    ...extra,
  });
  await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
  return nscp;
}

const get = (path: string) => request(URL).get(path).trustLocalhost(true);

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
        plugins: "check_warning, CHECK_OK",
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("lists only the exposed queries", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(200);
      expect(res.body).toEqual({ plugins: ["check_ok", "check_warning"] });
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

    it("answers a plain 403", async () => {
      const res = await get(`/api/plugins?token=${TOKEN}`).expect(403);
      expect(res.text).not.toContain("check_ok");
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
      const deadline = Date.now() + 30_000;
      while (
        Date.now() < deadline &&
        !nscp.capturedStdout().includes("NCPA listener has NOT been started")
      ) {
        await new Promise((r) => setTimeout(r, 250));
      }
      expect(nscp.capturedStdout()).toContain(
        "refusing to start the NCPA listener in cleartext HTTP",
      );
      await expect(nscp.waitForPort(PORT, { timeoutMs: 2_000 })).rejects.toThrow();
    });
  });
});
