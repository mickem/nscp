import * as fs from "fs";
import * as https from "https";
import * as os from "os";
import * as path from "path";

import { NscpInstance, onWindows, type SettingsTree } from "@fixtures/index";

import { NrpeClient } from "./nrpe-client";
// Type only: rest-client loads undici, which needs Node 20, while the regular
// suites that use this fixture also run on Node 18 (the Debian/Ubuntu
// integration job installs the distro's nodejs). It is loaded on first use.
import type { RestClient } from "./rest-client";

/**
 * One agent with every kind of check engine loaded, for the scenarios that
 * are about the core and the engines not blocking each other:
 *
 * | engine       | fast check    | slow check (holds a marker while it runs)           |
 * | ------------ | ------------- | --------------------------------------------------- |
 * | C++ module   | `check_ok`    | `ext_sleep` (CheckExternalScripts, a child process) |
 * | C++ + child  | `ext_ok`      |                                                     |
 * | Python       | `py_ok`       | `py_sleep` (time.sleep)                             |
 * | Lua          | `lua_ok`      | `lua_sleep` (nscp.sleep)                            |
 * | Lua -> Py    | `lua_via_py`  | `lua_via_py_sleep`                                  |
 * | Py -> Lua    | `py_via_lua`  | `py_via_lua_sleep`                                  |
 * | Lua -> C++   | `lua_via_cpp` |                                                     |
 *
 * Every fast check echoes a caller-chosen token back in a message that names
 * the path it took (see `FAST_CHECKS`), so a worker can tell a right answer
 * from one crossed with a concurrent request. Every slow check takes its
 * duration in ms and a marker name, and holds `<markerDir>/<name>.running`
 * while it runs, so a scenario can see several of them inside the agent at the
 * same moment instead of inferring it from a clock.
 *
 * Driven over NRPE (plain TCP, `allow arguments`), whose socket server runs a
 * real worker pool, and over REST.
 *
 * The ports are knobs because a machine with an installed agent has 5666 and
 * 8443 taken: NSCP_STRESS_NRPE_PORT (default 15666) and NSCP_STRESS_WEB_PORT
 * (default 8443, the port every REST suite uses).
 */

const FIXTURES = path.resolve(__dirname, "..", "fixtures");

export const NRPE_PORT = Number(process.env.NSCP_STRESS_NRPE_PORT ?? 15666);
export const WEB_PORT = Number(process.env.NSCP_STRESS_WEB_PORT ?? 8443);
export const WEB_URL = `https://127.0.0.1:${WEB_PORT}`;

/**
 * Log lines an engine agent produces by being set up, for expectCleanLog: the
 * port probes that wait for NRPE to listen connect and hang up without a
 * packet, which the server logs as a failed read. The load never does that -
 * every NRPE request sends a whole packet, and one that fails is counted by
 * the scenario itself.
 */
export const ENGINE_LOG_NOISE = [/nrpe Failed to read data: End of file/];

export interface FastCheck {
  /** What the check exercises, for the report's per-path counters. */
  path: string;
  command: string;
  /** NRPE arguments for `token`. */
  args: (token: string) => string[];
  /** REST query options for `token`. */
  restArgs: (token: string) => Record<string, string>;
  /** The exact message a correct answer carries. */
  expect: (token: string) => string;
}

function bare(p: string, command: string, expect: (t: string) => string): FastCheck {
  // REST passes each option as one `key=value` token, and an option with an
  // empty value as the bare key, so `?tok=` reaches a script handler as the
  // single positional argument `tok`.
  return { path: p, command, args: (t) => [t], restArgs: (t) => ({ [t]: "" }), expect };
}

/** The fast checks, one per engine and per cross-engine path. */
export const FAST_CHECKS: FastCheck[] = [
  {
    path: "cpp",
    command: "check_ok",
    args: (t) => [`message=${t}`],
    restArgs: (t) => ({ message: t }),
    expect: (t) => t,
  },
  bare("python", "py_ok", (t) => `py ${t}`),
  bare("lua", "lua_ok", (t) => `lua ${t}`),
  bare("lua>python", "lua_via_py", (t) => `lua>py ${t}`),
  bare("python>lua", "py_via_lua", (t) => `py>lua ${t}`),
  bare("lua>cpp", "lua_via_cpp", (t) => `lua>cpp ${t}`),
  bare("process", "ext_ok", (t) => `ext ${t}`),
];

export interface SlowCheck {
  path: string;
  command: string;
  /** The message a correct answer carries for `ms`. */
  expect: (ms: number) => string;
}

/** The slow checks; each takes `<ms> <marker>`. */
export const SLOW_CHECKS: SlowCheck[] = [
  { path: "process", command: "ext_sleep", expect: (ms) => `ext slept ${ms}` },
  { path: "python", command: "py_sleep", expect: (ms) => `py slept ${ms}` },
  { path: "lua", command: "lua_sleep", expect: (ms) => `lua slept ${ms}` },
  { path: "lua>python", command: "lua_via_py_sleep", expect: (ms) => `lua>py slept ${ms}` },
  { path: "python>lua", command: "py_via_lua_sleep", expect: (ms) => `py>lua slept ${ms}` },
];

export interface EngineAgent {
  nscp: NscpInstance;
  nrpe: NrpeClient;
  /** Where the slow checks put their markers. */
  markerDir: string;
  /** The markers present right now, by name. */
  running(): string[];
  /** A logged-in pooled REST client with `connections` connections (undici: Node 20+). */
  rest(connections: number): Promise<RestClient>;
  /**
   * One REST query on its own connection, over node:https - for the regular
   * suites, which must not depend on undici. Logs in on first use.
   */
  restQuery(
    command: string,
    options?: Record<string, string>,
  ): Promise<{ result: number; message: string }>;
}

/** GET `url`; resolves with the status and body. The agent's certificate is self-signed. */
function httpsGet(
  url: string,
  headers: Record<string, string>,
): Promise<{ status: number; body: string }> {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers, rejectUnauthorized: false, agent: false }, (res) => {
      let body = "";
      res.setEncoding("utf8");
      res.on("data", (chunk: string) => (body += chunk));
      res.on("end", () => resolve({ status: res.statusCode ?? 0, body }));
    });
    req.on("error", reject);
    req.setTimeout(120_000, () =>
      req.destroy(new Error(`no answer from ${url.split("?")[0]} in 120 s`)),
    );
  });
}

/** The two external scripts: `ext_ok <token>` and `ext_sleep <ms> <marker>`. */
function writeExternalScripts(dir: string, markerDir: string): { ok: string; sleep: string } {
  if (onWindows) {
    const ok = path.join(dir, "ext_ok.bat");
    const sleep = path.join(dir, "ext_sleep.bat");
    fs.writeFileSync(ok, ["@echo off", "echo ext %1", ""].join("\r\n"));
    // ping is the portable batch sleep: -n N waits N-1 seconds.
    fs.writeFileSync(
      sleep,
      [
        "@echo off",
        `echo running > "${markerDir}\\%2.running"`,
        "set /a n=%1/1000+1",
        "ping -n %n% 127.0.0.1 >nul",
        `del "${markerDir}\\%2.running"`,
        "echo ext slept %1",
        "",
      ].join("\r\n"),
    );
    return { ok: `cmd /c ${ok} $ARG1$`, sleep: `cmd /c ${sleep} $ARG1$ $ARG2$` };
  }
  const ok = path.join(dir, "ext_ok.sh");
  const sleep = path.join(dir, "ext_sleep.sh");
  fs.writeFileSync(ok, ["#!/bin/sh", 'echo "ext $1"', ""].join("\n"), { mode: 0o755 });
  fs.writeFileSync(
    sleep,
    [
      "#!/bin/sh",
      `echo running > "${markerDir}/$2.running"`,
      "sleep $(($1 / 1000))",
      `rm -f "${markerDir}/$2.running"`,
      'echo "ext slept $1"',
      "",
    ].join("\n"),
    { mode: 0o755 },
  );
  return { ok: `/bin/sh ${ok} $ARG1$`, sleep: `/bin/sh ${sleep} $ARG1$ $ARG2$` };
}

/**
 * Start an agent with every engine loaded, NRPE on NRPE_PORT and REST on
 * WEB_PORT. Each section of `extra` is merged over the defaults (a scenario's
 * schedules, a bigger NRPE pool); a section the defaults lack is added whole.
 */
export async function startEngineAgent(extra: SettingsTree = {}): Promise<EngineAgent> {
  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-stress-engines-"));
  const scripts = path.join(workDir, "scripts");
  fs.mkdirSync(path.join(scripts, "python"), { recursive: true });
  fs.mkdirSync(path.join(scripts, "lua"), { recursive: true });
  for (const [sub, file] of [
    ["python", "stress_engines.py"],
    ["lua", "stress_engines.lua"],
  ]) {
    fs.copyFileSync(path.join(FIXTURES, file), path.join(scripts, sub, file));
  }
  const nscp = new NscpInstance({ workDir, pathOverrides: { scripts } });
  const markerDir = nscp.scratch("markers");
  const ext = writeExternalScripts(nscp.scratch("ext"), markerDir);

  const tree: Record<string, Record<string, string | number | boolean>> = {
    "/modules": {
      CheckHelpers: "enabled",
      CheckExternalScripts: "enabled",
      PythonScript: "enabled",
      LUAScript: "enabled",
      NRPEServer: "enabled",
      WEBServer: "enabled",
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/WEB/server": { port: String(WEB_PORT) },
    "/settings/WEB/server/roles": { full: "*" },
    "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
    "/settings/NRPE/server": {
      port: String(NRPE_PORT),
      "use ssl": "false",
      "allow arguments": "true",
      // A slow check is a held connection, never a timed-out one.
      timeout: "300",
    },
    "/settings/external scripts": { "allow arguments": "true", timeout: "300" },
    "/settings/external scripts/scripts": { ext_ok: ext.ok, ext_sleep: ext.sleep },
    "/settings/python/scripts": { stress_engines: "stress_engines.py" },
    "/settings/lua/scripts": { stress_engines: "stress_engines.lua" },
    "/settings/stress": { markers: markerDir },
  };
  for (const [k, v] of Object.entries(extra)) {
    tree[k] = { ...(tree[k] ?? {}), ...(v as Record<string, string | number | boolean>) };
  }
  await nscp.configure(tree);

  await nscp.waitForPortFree(WEB_PORT, { timeoutMs: 30_000 });
  await nscp.waitForPortFree(NRPE_PORT, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(WEB_PORT, { timeoutMs: 30_000 });
  await nscp.waitForPort(NRPE_PORT, { timeoutMs: 30_000 });

  const nrpe = new NrpeClient(NRPE_PORT);
  // The scripts register their commands as they load, which can finish after
  // the ports are up: wait until every path answers before handing over.
  const deadline = performance.now() + 60_000;
  for (const c of FAST_CHECKS) {
    for (;;) {
      try {
        await nrpe.expect(c.command, c.args("ready"), c.expect("ready"));
        break;
      } catch (e) {
        if (performance.now() >= deadline) {
          throw new Error(`${c.command} never became ready: ${(e as Error).message}`);
        }
        await new Promise((r) => setTimeout(r, 500));
      }
    }
  }

  let restKey = "";
  return {
    nscp,
    nrpe,
    markerDir,
    running: () =>
      fs
        .readdirSync(markerDir)
        .filter((f) => f.endsWith(".running"))
        .map((f) => f.slice(0, -".running".length)),
    rest: async (connections: number) => {
      const { RestClient: Client } = await import("./rest-client");
      const c = new Client(connections, WEB_URL);
      await c.login();
      return c;
    },
    restQuery: async (command: string, options: Record<string, string> = {}) => {
      if (!restKey) {
        const auth = Buffer.from("admin:default-password").toString("base64");
        const login = await httpsGet(`${WEB_URL}/api/v1/login`, { authorization: `Basic ${auth}` });
        const key = (JSON.parse(login.body) as { key?: string }).key;
        if (login.status !== 200 || !key)
          throw new Error(`login failed: HTTP ${login.status} ${login.body.slice(0, 200)}`);
        restKey = key;
      }
      const qs = new URLSearchParams(options).toString();
      const r = await httpsGet(
        `${WEB_URL}/api/v1/queries/${command}/commands/execute${qs ? `?${qs}` : ""}`,
        {
          authorization: `Bearer ${restKey}`,
        },
      );
      if (r.status !== 200)
        throw new Error(`${command} -> HTTP ${r.status}: ${r.body.slice(0, 200)}`);
      const body = JSON.parse(r.body) as { result: number; lines?: { message: string }[] };
      return { result: body.result, message: body.lines?.[0]?.message ?? "" };
    },
  };
}
