/**
 * A fleet bundle that ships scripts, one of each kind, and the `script root`
 * that makes the agent run them from where the sync staged them.
 *
 * The bundle carries a Python script, a Lua script and an external script,
 * each depending on a second file delivered beside it: the Python and Lua
 * scripts import a helper from their `lib` folder, and the external script
 * calls a sibling by the relative `scripts/...` path. Its config sets
 * `[/settings/default] script root = fleet`, so every script module reads
 * the tree under `${fleet-folder}` - `scripts/python`, `scripts/lua` and, for
 * CheckExternalScripts, `scripts/` with the fleet folder as the working
 * directory. Nothing in the bundle names a host path, which is what makes it
 * the same bundle on Windows, Linux and macOS.
 *
 * The flow is the one fleet-sync.test.ts covers in detail (enroll, poll,
 * verify, stage, render, reload); this suite only cares that the scripts run
 * afterwards, so it drives them over REST once the configuration reload has
 * registered their commands. The modules themselves are enabled locally: a
 * bundle can enable a module in fleet.ini but cannot load one into a running
 * agent, which is also how the Azure estate roles do it.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import request from "supertest";
import {
  NscpInstance,
  makeZip,
  signBundle,
  makeCertPem,
  FLEET_TENANT_ID,
  onWindows,
  moduleBuiltHere,
  itWithModules,
  executeQuery,
  messageOf,
  OK,
  REST_URL,
  type QueryResult,
} from "@fixtures/index";

jest.setTimeout(240_000);

const PY_HELPER = "python helper from the fleet bundle";
const LUA_HELPER = "lua helper from the fleet bundle";
const EXT_HELPER = "external helper from the fleet bundle";

/**
 * The bundle. Both the .sh and the .bat are shipped; the config names the one
 * the platform runs. The sync writes files without an execute bit, so the
 * shell script is run through `sh` - and that spelling, `sh scripts/x.sh`, is
 * also what keeps the working directory the thing that resolves it.
 */
const scriptsZip = makeZip([
  { name: "bundle.toml", data: 'name = "scripted"\nversion = "1.0"\n' },
  {
    name: "config.json",
    data: JSON.stringify({
      settings: {
        default: { "script root": "fleet" },
        python: { scripts: { fleet_py: "fleet_check.py" } },
        lua: { scripts: { fleet_lua: "fleet_check.lua" } },
        "external scripts": {
          scripts: { fleet_ext: onWindows ? "scripts\\fleet_a.bat" : "sh scripts/fleet_a.sh" },
        },
      },
    }),
  },
  {
    name: "scripts/python/fleet_check.py",
    data: [
      "from NSCP import Registry, status",
      "import fleet_helper",
      "",
      "def init(plugin_id, plugin_alias, script_alias):",
      "    Registry.get(plugin_id).simple_function('fleet_py', lambda a: (status.OK, fleet_helper.greeting()), 'From a fleet bundle')",
      "",
    ].join("\n"),
  },
  {
    name: "scripts/python/lib/fleet_helper.py",
    data: `def greeting():\n    return "${PY_HELPER}"\n`,
  },
  {
    name: "scripts/lua/fleet_check.lua",
    data: [
      'local helper = require("fleet_helper")',
      "Registry():simple_query('fleet_lua', function(command, args)",
      "\treturn 'ok', helper.greeting(), ''",
      "end, 'From a fleet bundle')",
      "",
    ].join("\n"),
  },
  {
    name: "scripts/lua/lib/fleet_helper.lua",
    data: `local M = {}\nfunction M.greeting()\n\treturn "${LUA_HELPER}"\nend\nreturn M\n`,
  },
  // a calls b by the relative path, which only resolves from the root.
  { name: "scripts/fleet_a.sh", data: "#!/bin/sh\nsh scripts/fleet_b.sh\n" },
  { name: "scripts/fleet_b.sh", data: `#!/bin/sh\necho "${EXT_HELPER}"\n` },
  { name: "scripts/fleet_a.bat", data: "@echo off\r\ncall scripts\\fleet_b.bat\r\n" },
  { name: "scripts/fleet_b.bat", data: `@echo off\r\necho ${EXT_HELPER}\r\n` },
]);
const scriptsSha = crypto.createHash("sha256").update(scriptsZip).digest("hex");

describe("fleet bundle with scripts of every kind", () => {
  let nscp: NscpInstance;
  let workDir: string;
  let server: http.Server;
  let baseUrl: string;
  let key: string;
  const reports: any[] = [];

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);

  const desiredState = {
    tenant_id: FLEET_TENANT_ID,
    state_hash: "h-scripted",
    next_poll_in_seconds: 1,
    merged_config_json: {},
    bundles: [
      {
        id: "b-scripted",
        name: "scripted",
        version: "1.0",
        sha256: scriptsSha,
        format: "plain",
        signature: signBundle(signingKeys.privateKey, {
          id: "b-scripted",
          name: "scripted",
          version: "1.0",
          sha256: scriptsSha,
        }),
        url: "/agent/v1/bundles/b-scripted",
        priority: 100,
      },
    ],
  };

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-fleet-scripts-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    await nscp.configure({
      "/modules": {
        CheckExternalScripts: "enabled",
        LUAScript: "enabled",
        ...(moduleBuiltHere("PythonScript") ? { PythonScript: "enabled" } : {}),
        WEBServer: "enabled",
      },
      "/settings/default": { password: "default-password", "allowed hosts": "127.0.0.1,::1" },
      "/settings/WEB/server/roles": { full: "*" },
      "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
    });

    server = http.createServer((req, res) => {
      let raw = "";
      req.on("data", (chunk) => (raw += chunk));
      req.on("end", () => {
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
          if (parsed.searchParams.get("current_hash") === desiredState.state_hash) {
            res.writeHead(304, { "Content-Type": "application/json" });
            res.end(JSON.stringify({ next_poll_in_seconds: 1 }));
          } else {
            res.writeHead(200, { "Content-Type": "application/json" });
            res.end(JSON.stringify(desiredState));
          }
        } else if (parsed.pathname === "/agent/v1/bundles/b-scripted") {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(scriptsZip);
        } else if (parsed.pathname === "/agent/v1/state-report") {
          try {
            reports.push(JSON.parse(raw));
          } catch {
            /* not ours to judge here */
          }
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
        } else {
          // heartbeat, renew, facts: acknowledged and forgotten.
          res.writeHead(200, { "Content-Type": "application/json" });
          res.end("{}");
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
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    const login = await request(REST_URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    key = login.body.key as string;
  });

  afterAll(async () => {
    if (nscp) await nscp.stop();
    if (server) await new Promise<void>((resolve) => server.close(() => resolve()));
  });

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

  /** A fresh session: the reload that applies the bundle restarts the web server, which forgets its sessions. */
  async function login(): Promise<string> {
    const r = await request(REST_URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    key = r.body.key as string;
    return key;
  }

  /**
   * The check's result once the reload has registered the command: UNKNOWN
   * until then, and a 401/403 while the restarted web server has forgotten
   * the session, which is answered by logging in again.
   */
  async function ran(command: string, timeoutMs = 90_000): Promise<QueryResult> {
    const deadline = Date.now() + timeoutMs;
    let last: QueryResult | undefined;
    for (;;) {
      try {
        last = await executeQuery(key, command);
        if (last.result === OK) return last;
      } catch (e) {
        if (!/got 40[13]/.test(String(e))) throw e;
        await login().catch(() => undefined);
      }
      if (Date.now() >= deadline) {
        if (last) return last;
        throw new Error(`Timed out waiting for ${command} to answer`);
      }
      await new Promise((r) => setTimeout(r, 500));
    }
  }

  it("stages the bundle's script tree under the fleet folder and sets the root", async () => {
    await waitFor("the bundle to be applied", () =>
      reports.some((r) => r.applied_state_hash === "h-scripted"),
    );
    const report = reports.find((r) => r.applied_state_hash === "h-scripted");
    expect(report.bundles_installed).toEqual([{ id: "b-scripted", version: "1.0" }]);
    expect(report.errors).toEqual([]);

    const fleet = path.join(workDir, "fleet");
    for (const f of [
      "python/fleet_check.py",
      "python/lib/fleet_helper.py",
      "lua/fleet_check.lua",
      "lua/lib/fleet_helper.lua",
      "fleet_a.sh",
      "fleet_a.bat",
    ]) {
      expect(fs.existsSync(path.join(fleet, "scripts", f))).toBe(true);
    }
    const fleetIni = fs.readFileSync(path.join(fleet, "fleet.ini"), "utf8");
    expect(fleetIni).toContain("[/settings/default]");
    expect(fleetIni).toContain("script root=fleet");
    expect(fleetIni).toContain("[/settings/external scripts/scripts]");
  });

  it("runs the external script from the fleet root, so it reaches its sibling by relative path", async () => {
    const q = await ran("fleet_ext");
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toContain(EXT_HELPER);
  });

  it("loads the Lua script from the fleet tree, with its lib helper on the require path", async () => {
    const q = await ran("fleet_lua");
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toContain(LUA_HELPER);
  });

  itWithModules("PythonScript")(
    "loads the Python script from the fleet tree, with its lib helper importable",
    async () => {
      const q = await ran("fleet_py");
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain(PY_HELPER);
    },
  );

  it("lists the fleet-delivered scripts as the modules' own", async () => {
    await login();
    // The listings follow the root too: what `nscp lua list` and the web UI's
    // script pages show is the tree in force, helpers in `lib` left out.
    const lua = await request(REST_URL)
      .get("/api/v2/scripts/lua?all=true")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const luaNames = (lua.body as string[]).map((s) => s.replace(/\\/g, "/"));
    expect(luaNames).toContain("lua/fleet_check.lua");
    expect(luaNames).not.toContain("lua/lib/fleet_helper.lua");

    const ext = await request(REST_URL)
      .get("/api/v2/scripts/ext?all=true")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const extNames = (ext.body as string[]).map((s) => s.replace(/\\/g, "/"));
    expect(extNames).toContain("scripts/fleet_a.sh");

    if (moduleBuiltHere("PythonScript")) {
      const py = await request(REST_URL)
        .get("/api/v2/scripts/py?all=true")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      const pyNames = (py.body as string[]).map((s) => s.replace(/\\/g, "/"));
      expect(pyNames).toContain("python/fleet_check.py");
      expect(pyNames).not.toContain("python/lib/fleet_helper.py");
    }
  });

  it("moves the show/delete sandbox with the root", async () => {
    // fleet.ini sits two levels above <fleet>/scripts/lua: inside the fleet
    // folder, outside the scripts folder. The one-shot CLI reads the same
    // configuration, fleet.ini included, so it sees the root too.
    const inside = await nscp.run(["lua", "show", "--script", "fleet_check.lua"], {
      allowFailure: true,
    });
    expect(inside.all ?? `${inside.stdout}\n${inside.stderr}`).toContain('require("fleet_helper")');
    const outside = await nscp.run(["lua", "show", "--script", "../../fleet.ini"], {
      allowFailure: true,
    });
    expect(outside.all ?? `${outside.stdout}\n${outside.stderr}`).toContain("Not allowed outside");
    expect(outside.exitCode).not.toBe(0);
  });
});
