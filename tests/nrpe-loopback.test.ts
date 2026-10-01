/**
 * NRPEClient -> NRPEServer inside one agent: a check result has to survive the
 * round trip with SSL on and off and across the payload lengths the protocol
 * allows, up to the 1 MiB ceiling.
 *
 * Port of scripts/python/test_nrpe.py (and of its Lua twin
 * scripts/lua/test_nrpe.lua). A Lua `check_echo` registered on the agent
 * answers with whatever state and message it is handed, so a request that
 * comes back unchanged proves the client, the TLS layer and the v2 packet
 * (de)serializer on both ends. The packet is padded to the configured length
 * on the wire whatever the message size, which is where the buffer bugs live.
 *
 * Three ways in, all over REST:
 *   - nrpe_query against the configured `valid` target;
 *   - nrpe_query against an unconfigured target whose address, SSL mode and
 *     length come from the request (the legacy `test_rp` target);
 *   - a Lua `relay` check calling Core:query_forward('nrpe_forward', ...), the
 *     binding that ships a command and its arguments onto the wire as-is.
 *
 * The server and client are reconfigured for each SSL mode and length through
 * PUT /api/v2/settings and a reload of the running modules (the loadModuleEx
 * re-entry rule). A Lua `reload_modules` query does the reload; from inside a
 * dispatched call the core defers it to the scheduler, so each reconfiguration
 * is confirmed by a probe answering rather than by a sleep.
 *
 * Certificates come from generateCertChain(), so the TLS rows run with a real
 * server certificate rather than the anonymous `insecure` preset, and a
 * second, unrelated CA gives a target that must refuse the server.
 *
 * nrpe-tls, nrpe-verify-mode and plugin-threading drive the server from
 * check_nrpe or the `nscp nrpe` CLI; this suite is the only one that drives
 * NRPEClient's own targets.
 */
import * as fs from "fs";
import * as path from "path";

import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
  bundledSecurityFile,
  executeQuery,
  generateCertChain,
  messageOf,
  putSettings,
  setupQueryNscp,
  type QueryResult,
} from "@fixtures/index";

jest.setTimeout(600_000);

/** NRPE listener for this suite, off the 5666 default and plugin-threading's 15666. */
const NRPE_PORT = 25666;
/** Nothing listens here. */
const DEAD_PORT = 25669;
const SERVER_PATH = "/settings/NRPE/server";
const TARGET_PATH = "/settings/NRPE/client/targets/valid";

const SSL_MODES = [true, false];
const LENGTHS = [1024, 4096, 65536, 1048576];
const MATRIX: Array<[boolean, number]> = SSL_MODES.flatMap((ssl) =>
  LENGTHS.map((l): [boolean, number] => [ssl, l]),
);

const STATES: Array<[string, number]> = [
  ["ok", OK],
  ["warning", WARNING],
  ["critical", CRITICAL],
  ["unknown", UNKNOWN],
];

const LUA = `
-- Answers with the state and message it was handed: whatever went out over
-- NRPE has to come back unchanged.
local function check_echo(command, args)
  return args[1] or 'unknown', args[2] or '', ''
end

-- Forward check_echo over NRPE through Core:query_forward, which sets the
-- request header's command to nrpe_forward so the payload command and its
-- arguments reach the wire untouched.
local function relay(command, args)
  local code, msg = Core():query_forward('nrpe_forward', 'valid', 'check_echo', { args[1], args[2] })
  return code, msg, ''
end

-- Reload the named modules (REST passes a parameter with no value as a bare
-- argument). From inside a REST call the core hands the reload to the
-- scheduler, so this returns before it has applied.
local function reload_modules(command, args)
  local core = Core()
  for _, m in ipairs(args) do core:reload(m) end
  return 'ok', 'reload requested: ' .. table.concat(args, ',')
end

local reg = Registry()
reg:simple_function('check_echo', check_echo, 'echo the state and message back')
reg:simple_function('relay', relay, 'check_echo over NRPE via query_forward')
reg:simple_function('reload_modules', reload_modules, 'reload the named modules')
`;

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

describe("NRPE client -> server loopback", () => {
  let nscp: NscpInstance;
  let key: string;
  let certs: ReturnType<typeof generateCertChain>;
  let otherCa: ReturnType<typeof generateCertChain>;

  /** nrpe_query check_echo <state> <message> against a target. */
  function echo(
    state: string,
    message: string,
    target: Record<string, string> = { target: "valid" },
  ): Promise<QueryResult> {
    return executeQuery(key, "nrpe_query", {
      command: "check_echo",
      argument: [state, message],
      ...target,
    });
  }

  /** Request options that make an unconfigured target speak to our listener. */
  function adHoc(ssl: boolean, length: number): Record<string, string> {
    return {
      target: "test_rp",
      host: "127.0.0.1",
      port: String(NRPE_PORT),
      ssl: String(ssl),
      "payload-length": String(length),
    };
  }

  /**
   * Switch both ends to one SSL mode and length, reload them, and wait until
   * the configured target answers - which only happens once both reloads have
   * applied.
   */
  async function reconfigure(ssl: boolean, length: number): Promise<void> {
    await putSettings(key, SERVER_PATH, { "use ssl": ssl, "payload length": length });
    await putSettings(key, TARGET_PATH, { "use ssl": ssl, "payload length": length });
    const r = await executeQuery(key, "reload_modules", { NRPEServer: "", NRPEClient: "" });
    expect(r.result).toBe(OK);

    const deadline = Date.now() + 30_000;
    let last: QueryResult | undefined;
    for (;;) {
      const tag = `probe-${ssl}-${length}`;
      last = await echo("ok", tag);
      if (last.result === OK && messageOf(last) === tag) return;
      if (Date.now() >= deadline) {
        throw new Error(
          `NRPE loopback did not come up with ssl=${ssl} length=${length} within 30s: ${JSON.stringify(last)}`,
        );
      }
      await sleep(250);
    }
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    const script = path.join(nscp.scratch("lua"), "nrpe_loopback.lua");
    fs.writeFileSync(script, LUA);
    certs = generateCertChain({
      outDir: nscp.scratch("certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    otherCa = generateCertChain({
      outDir: nscp.scratch("other-ca"),
      caCommonName: "some-other-ca",
      signed: { unused: { commonName: "localhost", isServer: true } },
    });

    await nscp.waitForPortFree(NRPE_PORT, { timeoutMs: 30_000 });
    key = await setupQueryNscp(nscp, "LUAScript", {
      "/modules": {
        LUAScript: "enabled",
        WEBServer: "enabled",
        NRPEServer: "enabled",
        NRPEClient: "enabled",
      },
      "/settings/lua/scripts": { nrpe_loopback: script },
      [SERVER_PATH]: {
        port: String(NRPE_PORT),
        "allow arguments": "true",
        "use ssl": "true",
        "payload length": "1024",
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
        // certificate-path is a per-instance scratch directory with no DH
        // parameters in it; use the ones shipped with the build.
        dh: bundledSecurityFile("nrpe_dh_2048.pem"),
      },
      [TARGET_PATH]: {
        address: `127.0.0.1:${NRPE_PORT}`,
        "use ssl": "true",
        "payload length": "1024",
        timeout: "10",
      },
      // Verifies the server against the CA that issued its certificate.
      "/settings/NRPE/client/targets/trusted": {
        address: `127.0.0.1:${NRPE_PORT}`,
        "use ssl": "true",
        "payload length": "1024",
        ca: certs.ca.certPath,
        "verify mode": "peer-cert",
        timeout: "10",
      },
      // Verifies it against a CA that has never seen it.
      "/settings/NRPE/client/targets/wrong_ca": {
        address: `127.0.0.1:${NRPE_PORT}`,
        "use ssl": "true",
        "payload length": "1024",
        ca: otherCa.ca.certPath,
        "verify mode": "peer-cert",
        timeout: "10",
      },
      // Twice the server's length.
      "/settings/NRPE/client/targets/wrong_length": {
        address: `127.0.0.1:${NRPE_PORT}`,
        "use ssl": "true",
        "payload length": "2048",
        timeout: "10",
      },
      "/settings/NRPE/client/targets/dead": {
        address: `127.0.0.1:${DEAD_PORT}`,
        "use ssl": "true",
        "payload length": "1024",
        timeout: "5",
      },
    });
    await nscp.waitForPort(NRPE_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe.each(MATRIX)("ssl=%s, payload length %i", (ssl, length) => {
    beforeAll(async () => {
      await reconfigure(ssl, length);
    });

    it.each(STATES)("nrpe_query to the configured target carries %s", async (state, code) => {
      const message = `${state}-${ssl}-${length}`;
      const q = await echo(state, message);
      expect(q.result).toBe(code);
      expect(messageOf(q)).toBe(message);
    });

    it.each(STATES)("nrpe_query with the target in the request carries %s", async (state, code) => {
      const message = `${state}-adhoc-${length}`;
      const q = await echo(state, message, adHoc(ssl, length));
      expect(q.result).toBe(code);
      expect(messageOf(q)).toBe(message);
    });

    it("Core:query_forward relays a command and its arguments as-is", async () => {
      const message = `relayed-${ssl}-${length}`;
      const q = await executeQuery(key, "relay", { critical: "", [message]: "" });
      expect(q.result).toBe(CRITICAL);
      expect(messageOf(q)).toBe(message);
    });

    if (length >= 4096) {
      it("carries a message of a few kilobytes", async () => {
        const message = "x".repeat(3000);
        const q = await echo("warning", message);
        expect(q.result).toBe(WARNING);
        expect(messageOf(q)).toBe(message);
      });
    }
  });

  describe("targets that must not answer OK", () => {
    beforeAll(async () => {
      // The targets above are written for TLS at 1024 bytes.
      await reconfigure(true, 1024);
    });

    it("a target verifying the server's CA gets through", async () => {
      const q = await echo("ok", "verified", { target: "trusted" });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toBe("verified");
    });

    it("a target verifying against another CA refuses the server", async () => {
      const q = await echo("ok", "should not arrive", { target: "wrong_ca" });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).not.toBe("should not arrive");
    });

    it("a payload length the server does not use is UNKNOWN", async () => {
      const q = await echo("ok", "should not arrive", { target: "wrong_length" });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).not.toBe("should not arrive");
    });

    it("TLS against a plain-text server is UNKNOWN", async () => {
      await reconfigure(false, 1024);
      try {
        const q = await echo("ok", "should not arrive", adHoc(true, 1024));
        expect(q.result).toBe(UNKNOWN);
        expect(messageOf(q)).not.toBe("should not arrive");
      } finally {
        await reconfigure(true, 1024);
      }
    });

    it("an unreachable target is UNKNOWN", async () => {
      const q = await echo("ok", "nobody home", { target: "dead" });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).not.toBe("nobody home");
    });

    it("the agent still answers afterwards", async () => {
      const q = await echo("ok", "still here");
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toBe("still here");
    });
  });
});
