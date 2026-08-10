/**
 * Exercises `nscp enroll` (fleet onboarding) end-to-end against the real nscp
 * binary and a local fake fleet server.
 *
 * Each case runs the one-shot enroll command — `nscp enroll --server <url>
 * --token <token> ...` — against an in-process node HTTP server standing in
 * for the fleet server's public /enroll/v1 endpoint. This verifies the full
 * chain: CLI argument handling, Ed25519 key + CSR generation, the enrollment
 * POST, response parsing, retry/fatal error classification and durable state
 * file persistence. No REST web server or docker needed, so it runs anywhere
 * the binary builds.
 */
import http from "http";
import { AddressInfo } from "net";
import fs from "fs";
import os from "os";
import path from "path";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

interface SeenRequest {
  url: string;
  headers: http.IncomingHttpHeaders;
  body: any;
}

describe("nscp enroll (fleet onboarding CLI)", () => {
  let nscp: NscpInstance;
  let server: http.Server;
  let baseUrl: string;
  let requests: SeenRequest[];
  let responder: (req: http.IncomingMessage, res: http.ServerResponse) => void;

  /** A complete /enroll/v1 success payload; server_url is omitted on purpose
   * so the client's fallback to the --server url is exercised too. */
  const okResponse = () => ({
    cert_pem: "CERT",
    ca_pem: "CA",
    bundle_signing_pub_pem: "BUNDLE-KEY",
    mtls_url: "https://mtls.example.com:8443",
    mtls_server_cert_pem: "MTLS-CERT",
  });

  const respondOk: typeof responder = (_req, res) => {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify(okResponse()));
  };

  beforeAll(async () => {
    nscp = new NscpInstance();
    server = http.createServer((req, res) => {
      let raw = "";
      req.on("data", (chunk) => (raw += chunk));
      req.on("end", () => {
        let body: any = raw;
        try {
          body = JSON.parse(raw);
        } catch {
          /* keep raw */
        }
        requests.push({ url: req.url ?? "", headers: req.headers, body });
        responder(req, res);
      });
    });
    await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
    const address = server.address() as AddressInfo;
    baseUrl = `http://127.0.0.1:${address.port}`;
  });

  afterAll(async () => {
    await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  beforeEach(() => {
    requests = [];
    responder = respondOk;
  });

  /** Run `nscp enroll` with default server/token plus the given extra args. The
   * fake server speaks plain http, so --insecure is on by default; the tests
   * that exercise the insecure gate itself pass their own args. */
  async function enroll(extra: string[] = [], args: string[] = ["--server", baseUrl, "--token", "tok-1", "--insecure"]) {
    return nscp.run(["enroll", ...args, ...extra], { allowFailure: true });
  }

  function readState(file: string): any {
    return JSON.parse(fs.readFileSync(file, "utf8"));
  }

  it("enrolls successfully and persists the identity to --state-file", async () => {
    const stateFile = path.join(nscp.scratch("enroll_ok"), "agent-state.json");
    const r = await enroll(["--state-file", stateFile]);
    expect(r.exitCode).toBe(0);

    // The fake server saw one well-formed enrollment request.
    expect(requests).toHaveLength(1);
    expect(requests[0].url).toBe("/enroll/v1");
    expect(requests[0].headers["content-type"]).toMatch(/application\/json/);
    expect(requests[0].body.bootstrap_token).toBe("tok-1");
    expect(requests[0].body.csr_pem).toContain("BEGIN CERTIFICATE REQUEST");
    expect(requests[0].body.hostname).toBeTruthy();
    expect(requests[0].body.os).toBeTruthy();

    // The state file holds the server material plus the local private key.
    const state = readState(stateFile);
    expect(state.cert_pem).toBe("CERT");
    expect(state.ca_pem).toBe("CA");
    expect(state.bundle_signing_pub_pem).toBe("BUNDLE-KEY");
    expect(state.mtls_url).toBe("https://mtls.example.com:8443");
    expect(state.mtls_server_cert_pem).toBe("MTLS-CERT");
    expect(state.private_key_pem).toContain("BEGIN PRIVATE KEY");
    // server_url was omitted from the response: falls back to --server.
    expect(state.server_url).toBe(baseUrl);
  });

  it("passes --hostname and --os through to the enrollment request", async () => {
    const stateFile = path.join(nscp.scratch("enroll_tags"), "agent-state.json");
    const r = await enroll(["--state-file", stateFile, "--hostname", "web-01", "--os", "linux"]);
    expect(r.exitCode).toBe(0);
    expect(requests[0].body.hostname).toBe("web-01");
    expect(requests[0].body.os).toBe("linux");
  });

  it("defaults the state file to ${certificate-path}/agent-state.json", async () => {
    const expected = path.join(nscp.pathOverrides["certificate-path"], "agent-state.json");
    // A previous suite run (or test) may have left a state file behind.
    fs.rmSync(expected, { force: true });
    const r = await enroll();
    expect(r.exitCode).toBe(0);
    expect(fs.existsSync(expected)).toBe(true);
    expect(readState(expected).cert_pem).toBe("CERT");
  });

  it("refuses to overwrite an existing enrollment unless --force is given", async () => {
    const stateFile = path.join(nscp.scratch("enroll_force"), "agent-state.json");
    expect((await enroll(["--state-file", stateFile])).exitCode).toBe(0);

    const refused = await enroll(["--state-file", stateFile]);
    expect(refused.exitCode).not.toBe(0);
    expect(refused.all).toMatch(/already enrolled/i);
    // No new enrollment request was made (1 from setup above).
    expect(requests).toHaveLength(1);

    const forced = await enroll(["--state-file", stateFile, "--force"]);
    expect(forced.exitCode).toBe(0);
    expect(requests).toHaveLength(2);
  });

  it("treats a rejected token (403) as fatal: no retries, no state file", async () => {
    responder = (_req, res) => {
      res.writeHead(403, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "token already used" }));
    };
    const stateFile = path.join(nscp.scratch("enroll_403"), "agent-state.json");
    const r = await enroll(["--state-file", stateFile]);
    expect(r.exitCode).not.toBe(0);
    expect(r.all).toMatch(/new install command/i);
    expect(requests).toHaveLength(1);
    expect(fs.existsSync(stateFile)).toBe(false);
  });

  it("honors Retry-After on 429 and succeeds on the second attempt", async () => {
    let calls = 0;
    responder = (req, res) => {
      if (++calls === 1) {
        res.writeHead(429, { "Retry-After": "1" });
        res.end("slow down");
        return;
      }
      respondOk(req, res);
    };
    const stateFile = path.join(nscp.scratch("enroll_429"), "agent-state.json");
    const started = Date.now();
    const r = await enroll(["--state-file", stateFile]);
    expect(r.exitCode).toBe(0);
    expect(requests).toHaveLength(2);
    // Retry-After of 1s (plus up to 1s jitter) must actually be waited out.
    expect(Date.now() - started).toBeGreaterThanOrEqual(1000);
    expect(fs.existsSync(stateFile)).toBe(true);
  });

  it("gives up after --retries transient failures", async () => {
    responder = (_req, res) => {
      res.writeHead(500);
      res.end("boom");
    };
    const stateFile = path.join(nscp.scratch("enroll_500"), "agent-state.json");
    const r = await enroll(["--state-file", stateFile, "--retries", "2"]);
    expect(r.exitCode).not.toBe(0);
    expect(r.all).toMatch(/try again later/i);
    expect(requests).toHaveLength(2);
    expect(fs.existsSync(stateFile)).toBe(false);
  });

  it("requires --server and --token before contacting anything", async () => {
    const r = await enroll([], []);
    expect(r.exitCode).not.toBe(0);
    expect(r.all).toMatch(/--server and --token are required/i);
    expect(requests).toHaveLength(0);
  });

  it("refuses plain-HTTP enrollment unless --insecure is given", async () => {
    // The bootstrap token is a credential; sending it over http:// would leak
    // it in cleartext, so the gate must fire before any network call is made.
    const stateFile = path.join(nscp.scratch("enroll_insecure"), "agent-state.json");
    fs.rmSync(stateFile, { force: true });
    const r = await enroll(["--state-file", stateFile], ["--server", baseUrl, "--token", "tok-1"]);
    expect(r.exitCode).not.toBe(0);
    expect(r.all).toMatch(/plain http/i);
    expect(r.all).toMatch(/--insecure|https/i);
    expect(requests).toHaveLength(0);
    expect(fs.existsSync(stateFile)).toBe(false);
  });

  it("allows plain-HTTP enrollment with --insecure", async () => {
    const stateFile = path.join(nscp.scratch("enroll_insecure_ok"), "agent-state.json");
    const r = await enroll(["--state-file", stateFile], ["--server", baseUrl, "--token", "tok-1", "--insecure"]);
    expect(r.exitCode).toBe(0);
    expect(requests).toHaveLength(1);
  });

  it("does not apply the plain-HTTP gate to an https url", async () => {
    // https needs no --insecure. Against this http-only server it fails at the
    // TLS layer - but crucially not with the plain-HTTP refusal.
    const stateFile = path.join(nscp.scratch("enroll_https_nogate"), "agent-state.json");
    fs.rmSync(stateFile, { force: true });
    const r = await enroll(["--state-file", stateFile, "--retries", "1"], ["--server", baseUrl.replace("http://", "https://"), "--token", "tok-1"]);
    expect(r.exitCode).not.toBe(0);
    expect(r.all).not.toMatch(/plain http/i);
  });

  it("refuses --verify none over https unless --insecure is given", async () => {
    // Enrollment is where the agent learns which certificate to pin and which
    // key signs its bundles. Taking that over an unverified channel lets an
    // on-path attacker supply both, so it has to be asked for explicitly - and
    // the gate must fire before any network call.
    const stateFile = path.join(nscp.scratch("enroll_verify_none"), "agent-state.json");
    fs.rmSync(stateFile, { force: true });
    const r = await enroll(
      ["--state-file", stateFile, "--verify", "none"],
      ["--server", baseUrl.replace("http://", "https://"), "--token", "tok-1"],
    );
    expect(r.exitCode).not.toBe(0);
    expect(r.all).toMatch(/without verifying the fleet server certificate/i);
    expect(r.all).toMatch(/--insecure|--ca/i);
    expect(requests).toHaveLength(0);
    expect(fs.existsSync(stateFile)).toBe(false);
  });

  it("allows --verify none over https with --insecure, but warns", async () => {
    // Against this http-only server the call still fails at the TLS layer; what
    // matters is that it got past the gate and said so.
    const stateFile = path.join(nscp.scratch("enroll_verify_none_ok"), "agent-state.json");
    fs.rmSync(stateFile, { force: true });
    const r = await enroll(
      ["--state-file", stateFile, "--verify", "none", "--retries", "1"],
      ["--server", baseUrl.replace("http://", "https://"), "--token", "tok-1", "--insecure"],
    );
    expect(r.all).not.toMatch(/Refusing to enroll without verifying/i);
    expect(r.all).toMatch(/WARNING: enrolling without verifying/i);
  });

  it("rejects a server url that is not a url, without contacting anything", async () => {
    for (const url of ["fleet.example.com", "not a url", "/enroll"]) {
      const stateFile = path.join(nscp.scratch("enroll_badurl"), "agent-state.json");
      fs.rmSync(stateFile, { force: true });
      const r = await enroll(["--state-file", stateFile], ["--server", url, "--token", "tok-1"]);
      expect(r.exitCode).not.toBe(0);
      expect(r.all).toMatch(/invalid server url/i);
      expect(fs.existsSync(stateFile)).toBe(false);
      expect(requests).toHaveLength(0);
    }
  });

  it("fails cleanly when --ca points at something unusable", async () => {
    // The CA is loaded by the TLS context before any connection: a missing or
    // malformed file has to surface as an enrollment failure, not a crash, and
    // must not leave a half written identity behind.
    const dir = nscp.scratch("enroll_ca");
    const missing = path.join(dir, "no-such-ca.pem");
    const garbage = path.join(dir, "garbage-ca.pem");
    fs.writeFileSync(garbage, "this is not a certificate\n");
    for (const ca of [missing, garbage, dir]) {
      const stateFile = path.join(dir, "agent-state.json");
      fs.rmSync(stateFile, { force: true });
      // https so the TLS context (and therefore the CA) is actually built.
      const r = await enroll(["--state-file", stateFile, "--ca", ca, "--retries", "1"], ["--server", baseUrl.replace("http://", "https://"), "--token", "tok-1"]);
      expect(r.exitCode).not.toBe(0);
      expect(r.all).toMatch(/failed/i);
      expect(fs.existsSync(stateFile)).toBe(false);
    }
  });

  it("creates the directory for --state-file when it does not exist yet", async () => {
    // The installer points --state-file at a path under a directory it has not
    // created yet; enrollment is expected to create it (0600 file inside).
    const stateFile = path.join(nscp.scratch("enroll_mkdir"), "nested", "deeper", "agent-state.json");
    const r = await enroll(["--state-file", stateFile]);
    expect(r.exitCode).toBe(0);
    expect(fs.existsSync(stateFile)).toBe(true);
    if (process.platform !== "win32") {
      expect(fs.statSync(stateFile).mode & 0o777).toBe(0o600);
    }
  });

  it("expands path tokens in --state-file", async () => {
    const stateFile = "${certificate-path}/token-expanded-state.json";
    const expected = path.join(nscp.pathOverrides["certificate-path"], "token-expanded-state.json");
    fs.rmSync(expected, { force: true });
    expect((await enroll(["--state-file", stateFile])).exitCode).toBe(0);
    expect(fs.existsSync(expected)).toBe(true);
  });

  it("writes the fleet.ini include and a placeholder, but enables no module", async () => {
    // The core starts the sync loop from the manifest alone, so the only
    // settings change enrollment may make is the include. Own instance: this
    // one needs ${shared-path} pointed somewhere writable to see the
    // placeholder land.
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-include-"));
    const own = new NscpInstance({ workDir: dir, pathOverrides: { "shared-path": dir } });
    expect((await own.run(["enroll", "--server", baseUrl, "--token", "tok-1", "--insecure"], { allowFailure: true })).exitCode).toBe(0);

    const ini = fs.readFileSync(own.settingsFile, "utf8");
    expect(ini).toMatch(/\[\/includes\]/);
    expect(ini).toMatch(/fleet\s*=/);
    // Unexpanded on purpose, so the configuration stays relocatable.
    expect(ini).toContain("${shared-path}/fleet/fleet.ini");
    expect(ini).not.toMatch(/\[\/modules\]/);

    const placeholder = path.join(dir, "fleet", "fleet.ini");
    expect(fs.existsSync(placeholder)).toBe(true);
    expect(fs.readFileSync(placeholder, "utf8")).toMatch(/managed by the fleet sync/i);
    fs.rmSync(dir, { recursive: true, force: true });
  });
});
