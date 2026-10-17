/**
 * The Azure estate's `scripts` role, run locally on every CI build.
 *
 * live/estate.test.ts only runs when someone provisions the estate
 * (build/powershell/run-estate.ps1), so between estate runs nothing would
 * notice a role whose bundle, checks or REST reads no longer match the agent.
 * This suite closes that gap: it builds the role's bundle the way the estate
 * builds it (src/estate-roles.ts, checked against the estate's own
 * PowerShell where PowerShell is installed), serves it from a fake fleet
 * server to a local agent, and judges the role's `checks` and `rest` entries
 * with the functions the estate suite uses. What is left for an estate run
 * to prove is the real infrastructure: the fleet server, the packages and
 * the operating systems.
 *
 * The role's modules are enabled locally, as the estate's installer does: a
 * bundle can enable a module in fleet.ini but cannot load one into a running
 * agent.
 */
import http from "http";
import { AddressInfo } from "net";
import crypto from "crypto";
import fs from "fs";
import os from "os";
import path from "path";
import zlib from "zlib";
import { spawnSync } from "child_process";
import request from "supertest";
import {
  NscpInstance,
  makeZip,
  bundleEntry,
  makeCertPem,
  FLEET_TENANT_ID,
  onWindows,
  describeWithModules,
  executeQuery,
  messageOf,
  perfOf,
  liveRequest,
  type LiveTarget,
  REST_URL,
  type QueryResult,
} from "@fixtures/index";
import {
  ESTATE_ROLES_DIR,
  applies,
  judgeCheck,
  attemptRoleRest,
  restLabel,
  readRole,
  roleBundleConfig,
  roleBundleEntries,
} from "@fixtures/estate-roles";

jest.setTimeout(240_000);

const ROLE = "scripts";
// What ${ROLE_PASSWORD} in the role's bundle settings is filled in with. Not a
// credential anything checks: the bundle it lands in is digested (SHA-256)
// because the fleet protocol identifies a bundle by its content hash, which a
// scanner reads as a password being hashed if the value is named one.
const ROLE_SECRET = "local-role-secret";
const BUNDLE_VERSION = "1.0";

const role = readRole(ROLE);
const family = onWindows ? "windows" : "linux";
const checks = (role.checks ?? []).filter((c) => applies(c, family));
const rests = (role.rest ?? []).filter((r) => applies(r, family));
const entries = roleBundleEntries(role, BUNDLE_VERSION, ROLE_SECRET);
const bundleZip = makeZip(entries);

/** The entries of a zip, store or deflate: enough to read what ZipArchive writes. */
function readZip(zip: Buffer): Map<string, Buffer> {
  const out = new Map<string, Buffer>();
  let eocd = zip.length - 22;
  while (eocd >= 0 && zip.readUInt32LE(eocd) !== 0x06054b50) eocd--;
  if (eocd < 0) throw new Error("not a zip: no end of central directory");
  let at = zip.readUInt32LE(eocd + 16);
  const count = zip.readUInt16LE(eocd + 10);
  for (let i = 0; i < count; i++) {
    const method = zip.readUInt16LE(at + 10);
    const size = zip.readUInt32LE(at + 20);
    const nameLen = zip.readUInt16LE(at + 28);
    const extraLen = zip.readUInt16LE(at + 30);
    const commentLen = zip.readUInt16LE(at + 32);
    const local = zip.readUInt32LE(at + 42);
    const name = zip.toString("utf8", at + 46, at + 46 + nameLen);
    const dataAt = local + 30 + zip.readUInt16LE(local + 26) + zip.readUInt16LE(local + 28);
    const raw = zip.subarray(dataAt, dataAt + size);
    out.set(name, method === 8 ? zlib.inflateRawSync(raw) : Buffer.from(raw));
    at += 46 + nameLen + extraLen + commentLen;
  }
  return out;
}

/** Windows PowerShell where it is the system's, else pwsh when installed. */
function powershell(): string | undefined {
  const candidates = onWindows ? ["powershell.exe", "pwsh"] : ["pwsh"];
  return candidates.find((exe) => {
    const r = spawnSync(exe, ["-NoProfile", "-Command", "exit 0"], { timeout: 60_000 });
    return !r.error && r.status === 0;
  });
}

describe(`estate role '${ROLE}': bundle`, () => {
  it("is built with every file the role ships and the settings it declares", () => {
    const names = entries.map((e) => e.name);
    expect(names).toEqual(
      expect.arrayContaining([
        "bundle.toml",
        "config.json",
        "scripts/python/fleet_check.py",
        "scripts/lua/lib/fleet_helper.lua",
        "scripts/fleet_a.sh",
        "scripts/fleet_a.bat",
      ]),
    );
    const config = roleBundleConfig(role, ROLE_SECRET) as any;
    expect(config.settings.default["script root"]).toBe("fleet");
    for (const m of role.modules ?? []) expect(config.modules[m]).toBe("enabled");
  });

  // The estate uploads the zip its PowerShell builds; this suite serves the
  // one src/estate-roles.ts builds. Where PowerShell is installed (always on
  // Windows), the two must agree, or this suite tests a different bundle
  // than the estate ships.
  const ps = powershell();
  (ps ? it : it.skip)("matches what the estate's PowerShell builds", () => {
    const out = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "nscp-estate-bundle-")), "b.zip");
    const api = path.join(ESTATE_ROLES_DIR, "..", "estate-api.ps1").replace(/'/g, "''");
    const script = [
      "$ErrorActionPreference = 'Stop'",
      `. '${api}'`,
      `$r = (Get-EstateRoleCatalog)['${ROLE}']`,
      `$c = ConvertTo-EstateBundleConfig -Role $r -RolePassword '${ROLE_SECRET}'`,
      `$z = New-EstateBundleZip -Name '${ROLE}' -Version '${BUNDLE_VERSION}' -Config $c -ScriptsDir $r.BundleScriptsDir`,
      `[System.IO.File]::WriteAllBytes('${out.replace(/'/g, "''")}', $z)`,
    ].join("; ");
    const r = spawnSync(ps!, ["-NoProfile", "-NonInteractive", "-Command", script], {
      encoding: "utf8",
      timeout: 120_000,
    });
    expect(`${r.status} ${r.stderr ?? ""}`.trim()).toBe("0");

    const theirs = readZip(fs.readFileSync(out));
    const ours = new Map(entries.map((e) => [e.name, Buffer.from(e.data)]));
    expect([...theirs.keys()].sort()).toEqual([...ours.keys()].sort());
    for (const [name, data] of ours) {
      if (name === "config.json")
        expect(JSON.parse(theirs.get(name)!.toString("utf8"))).toEqual(
          JSON.parse(data.toString("utf8")),
        );
      else
        expect({ name, data: theirs.get(name)!.toString("utf8") }).toEqual({
          name,
          data: data.toString("utf8"),
        });
    }
  });
});

describeWithModules(...(role.modules ?? []))(`estate role '${ROLE}' on a local agent`, () => {
  let nscp: NscpInstance;
  let server: http.Server;
  let baseUrl: string;
  let key: string;
  let workDir: string;
  const reports: any[] = [];

  const signingKeys = crypto.generateKeyPairSync("ed25519");
  const signingPubPem = signingKeys.publicKey.export({ type: "spki", format: "pem" }).toString();
  const agentCertPem = makeCertPem(90);
  const desiredState = {
    tenant_id: FLEET_TENANT_ID,
    state_hash: `h-${ROLE}`,
    next_poll_in_seconds: 1,
    merged_config_json: {},
    bundles: [bundleEntry(ROLE, bundleZip, signingKeys.privateKey, { version: BUNDLE_VERSION })],
  };

  beforeAll(async () => {
    workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-estate-role-"));
    nscp = new NscpInstance({ workDir, pathOverrides: { "shared-path": workDir } });
    await nscp.configure({
      "/modules": {
        ...Object.fromEntries((role.modules ?? []).map((m) => [m, "enabled"])),
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
        res.setHeader("Content-Type", "application/json");
        if (req.method === "POST" && parsed.pathname === "/enroll/v1") {
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
            res.writeHead(304);
            res.end(JSON.stringify({ next_poll_in_seconds: 1 }));
          } else res.end(JSON.stringify(desiredState));
        } else if (parsed.pathname === `/agent/v1/bundles/${ROLE}`) {
          res.writeHead(200, { "Content-Type": "application/zip" });
          res.end(bundleZip);
        } else {
          if (parsed.pathname === "/agent/v1/state-report") {
            try {
              reports.push(JSON.parse(raw));
            } catch {
              /* not ours to judge here */
            }
          }
          // heartbeat, renew, facts: acknowledged and forgotten.
          res.end("{}");
        }
      });
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    baseUrl = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;

    const enrolled = await nscp.run(
      ["enroll", "--server", baseUrl, "--token", "tok-estate", "--insecure"],
      { allowFailure: true },
    );
    expect(enrolled.exitCode).toBe(0);

    await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await login();

    const started = Date.now();
    while (!reports.some((r) => r.applied_state_hash === desiredState.state_hash)) {
      if (Date.now() - started > 90_000) throw new Error("the role's bundle was never applied");
      await new Promise((r) => setTimeout(r, 200));
    }
  });

  afterAll(async () => {
    if (nscp) await nscp.stop();
    if (server) await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  async function login(): Promise<void> {
    const r = await request(REST_URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    key = r.body.key as string;
  }

  /**
   * Retry until `attempt` reports no failure or the wait is over: the reload
   * that applies the bundle restarts the web server, which drops connections
   * and forgets sessions, and registers the scripts' commands only once done.
   */
  async function eventually(
    attempt: () => Promise<string | undefined>,
  ): Promise<string | undefined> {
    const deadline = Date.now() + 90_000;
    for (;;) {
      let why: string | undefined;
      try {
        why = await attempt();
      } catch (e) {
        why = `request failed: ${String(e).split("\n")[0]}`;
        await login().catch(() => undefined);
      }
      if (!why || Date.now() >= deadline) return why;
      await new Promise((r) => setTimeout(r, 500));
    }
  }

  it("applied the bundle without errors", () => {
    const report = reports.find((r) => r.applied_state_hash === desiredState.state_hash);
    expect(report.errors).toEqual([]);
    expect(report.bundles_installed).toEqual([{ id: ROLE, version: BUNDLE_VERSION }]);
  });

  it.each(checks.map((c) => [c.command, c] as const))("check %s", async (_name, check) => {
    let last: QueryResult | undefined;
    const why = await eventually(async () => {
      last = await executeQuery(key, check.command, check.args);
      return judgeCheck(check, last.result, Object.keys(perfOf(last)), messageOf(last));
    });
    expect(why ? `${why} :: ${last ? messageOf(last) : ""}` : undefined).toBeUndefined();
  });

  it.each(rests.map((r) => [restLabel(r), r] as const))("%s", async (_name, rest) => {
    // `key` is read on every attempt: a lost session logs in again.
    const target = (): LiveTarget => ({ base: REST_URL, user: "admin", key, insecure: false });
    const { why, res } = await attemptRoleRest(
      rest,
      () => liveRequest(target(), rest.method ?? "GET", rest.path, rest.body),
      login,
      90_000,
      500,
    );
    const text = res ? res.text.slice(0, 500) : "";
    expect(why ? `${why} :: ${text}` : undefined).toBeUndefined();
  });

  // The same add / list / show / delete through the command line, which an
  // estate run cannot drive on a VM. A one-shot `nscp <runtime>` reads the
  // configuration the agent runs with - fleet.ini and its `script root`
  // included - so each script must land in, and leave, the fleet tree.
  const cliRuntimes = [
    {
      cli: "lua",
      folder: "lua",
      file: "estate_cli.lua",
      data: "-- added through the CLI\n",
      alias: "estate_cli",
      recorded: "lua/estate_cli.lua",
    },
    {
      cli: "py",
      folder: "python",
      file: "estate_cli.py",
      data: "# added through the CLI\ndef init(plugin_id, plugin_alias, script_alias):\n    pass\n",
      alias: "estate_cli.py",
      recorded: "python/estate_cli.py",
    },
    {
      cli: "ext-scr",
      folder: "",
      file: "estate_cli.sh",
      data: "#!/bin/sh\n# added through the CLI\necho added\n",
      alias: "estate_cli",
      recorded: "scripts/estate_cli.sh",
    },
  ];
  describe.each(cliRuntimes)("nscp $cli under the fleet root", (rt) => {
    const inFleet = () => path.join(workDir, "fleet", "scripts", rt.folder, rt.file);
    // What `list` prints and `show` takes: relative to the runtime's folder,
    // or to the root for external scripts.
    const listed = rt.folder ? `${rt.folder}/${rt.file}` : `scripts/${rt.file}`;
    const shown = rt.folder ? rt.file : listed;
    const cli = async (...args: string[]) => {
      const r = await nscp.run([rt.cli, ...args], { allowFailure: true });
      return { code: r.exitCode, out: (r.all ?? `${r.stdout}\n${r.stderr}`).replace(/\\+/g, "/") };
    };

    it("add --import copies the script into the fleet tree and records it relative to the root", async () => {
      // An import source must be inside the script root or ${shared-path},
      // which is the work directory here.
      const incoming = path.join(workDir, "incoming");
      fs.mkdirSync(incoming, { recursive: true });
      const src = path.join(incoming, rt.file);
      fs.writeFileSync(src, rt.data);
      const r = await cli("add", "--script", rt.file, "--import", src);
      expect(r.out).toContain(`Added ${rt.alias} as ${rt.recorded}`);
      expect(fs.existsSync(inFleet())).toBe(true);
    });

    it("list shows it", async () => {
      expect((await cli("list")).out).toContain(listed);
    });

    it("show reads it back", async () => {
      expect((await cli("show", "--script", shown)).out).toContain("added through the CLI");
    });

    it("delete removes it from the fleet tree", async () => {
      if (rt.cli === "ext-scr") {
        // CheckExternalScripts deletes a command definition by its alias and
        // leaves the file; the file goes by its path.
        expect((await cli("delete", "--script", rt.alias)).out).toContain(
          "Script definition has been removed",
        );
        expect(fs.existsSync(inFleet())).toBe(true);
        expect((await cli("delete", "--script", listed)).out).toContain("Script file was removed");
      } else {
        const r = await cli("delete", "--script", rt.file);
        expect(r.out).toContain(`Deleted ${inFleet().replace(/\\+/g, "/")}`);
      }
      expect(fs.existsSync(inFleet())).toBe(false);
    });

    it("list no longer shows it", async () => {
      expect((await cli("list")).out).not.toContain(listed);
    });
  });
});
