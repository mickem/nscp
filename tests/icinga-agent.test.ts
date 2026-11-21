/**
 * IcingaAgent: NSClient++ as a native Icinga 2 agent.
 *
 * The parent here is a fake Icinga 2 master written in TypeScript rather than
 * an icinga2 container: it speaks the same wire protocol (JSON-RPC framed as
 * netstrings over mutual TLS, port 5665 in real life), runs its own CA with
 * node-forge and signs the agent's certificate the way Icinga does - from the
 * public key of the certificate the agent connected with. That keeps the suite
 * docker-free and deterministic, and lets it drive the cases a real master
 * never produces on demand (a check on the unverified bootstrap connection, a
 * parent presenting the wrong name). The master-side configuration in
 * docs/docs/scenarios/icinga-agent.md was verified against a real Icinga 2.16
 * master.
 *
 * Covered: the ticket exchange on the unverified first connection, the
 * verified reconnect, ifw-api argument expansion, plain CheckCommands, the
 * core permission policy seeing `IcingaAgent:<parent endpoint>`, refused
 * event commands, heartbeats, `allow arguments = false`, the parent-name
 * pin, TLS settings, and every way into the first connection: a pinned
 * `parent fingerprint`, a ca.crt installed up front, and `trust on first
 * use = false`.
 */
import * as crypto from "crypto";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import * as tls from "tls";
import * as forge from "node-forge";
import {
  describeWithModules,
  generateCertChain,
  itIf,
  itOnUnix,
  moduleBuiltHere,
  NscpInstance,
  onWindows,
  type CertBundle,
} from "@fixtures/index";

jest.setTimeout(180_000);

const PARENT = "icinga-master";
const AGENT = "nscp-agent-it";
const TICKET = "0123456789abcdef0123456789abcdef01234567";

type Json = Record<string, unknown>;

interface RpcMessage {
  method?: string;
  params?: Json;
}

/** One TLS connection from the agent, as the fake master sees it. */
class Session {
  readonly messages: RpcMessage[] = [];
  closed = false;
  private buffer = Buffer.alloc(0);

  constructor(readonly socket: tls.TLSSocket) {
    socket.on("data", (chunk: Buffer) => this.onData(chunk));
    socket.on("close", () => {
      this.closed = true;
    });
    socket.on("error", () => {
      this.closed = true;
    });
  }

  /** Whether the agent's certificate verified against the master's CA. */
  get authorized(): boolean {
    return this.socket.authorized;
  }

  get peerCn(): string {
    const cn = this.socket.getPeerCertificate()?.subject?.CN;
    return Array.isArray(cn) ? cn[0] : (cn ?? "");
  }

  send(method: string, params: Json): void {
    const json = Buffer.from(JSON.stringify({ jsonrpc: "2.0", method, params }), "utf8");
    this.socket.write(Buffer.concat([Buffer.from(`${json.length}:`), json, Buffer.from(",")]));
  }

  async waitFor(
    method: string,
    match: (p: Json) => boolean = () => true,
    timeoutMs = 20_000,
  ): Promise<Json> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const hit = this.messages.find((m) => m.method === method && match(m.params ?? {}));
      if (hit) return hit.params ?? {};
      if (this.closed) break;
      await sleep(100);
    }
    throw new Error(
      `no ${method} from the agent (got: ${this.messages.map((m) => m.method).join(", ") || "nothing"})`,
    );
  }

  /** Send an event::ExecuteCommand and wait for the matching event::CheckResult. */
  async execute(
    service: string,
    command: string,
    macros: Json,
    commandType = "check_command",
  ): Promise<Json> {
    this.send("event::ExecuteCommand", {
      command_type: commandType,
      command,
      host: "it-host",
      service,
      macros,
    });
    const result = await this.waitFor("event::CheckResult", (p) => p.service === service);
    // Consume it, so a later check of the same service waits for a new one.
    const index = this.messages.findIndex(
      (m) => m.method === "event::CheckResult" && m.params?.service === service,
    );
    this.messages.splice(index, 1);
    return result.cr as Json;
  }

  private onData(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    for (;;) {
      while (this.buffer.length > 0 && this.buffer[0] === 0x2c)
        this.buffer = this.buffer.subarray(1);
      const colon = this.buffer.indexOf(0x3a);
      if (colon <= 0) return;
      const length = Number(this.buffer.subarray(0, colon).toString("ascii"));
      if (this.buffer.length < colon + 1 + length + 1) return;
      const payload = this.buffer.subarray(colon + 1, colon + 1 + length).toString("utf8");
      this.buffer = this.buffer.subarray(colon + 1 + length + 1);
      this.messages.push(JSON.parse(payload) as RpcMessage);
    }
  }
}

/** A fake Icinga 2 master: its own CA, a server certificate, and a session per connection. */
class FakeIcingaMaster {
  readonly sessions: Session[] = [];
  private server?: tls.Server;
  private readonly caKey: forge.pki.rsa.PrivateKey;
  private readonly caCert: forge.pki.Certificate;

  constructor(readonly pki: CertBundle) {
    this.caKey = forge.pki.privateKeyFromPem(pki.ca.keyPem) as forge.pki.rsa.PrivateKey;
    this.caCert = forge.pki.certificateFromPem(pki.ca.certPem);
  }

  async listen(serverCert: string): Promise<number> {
    const pair = this.pki.signed[serverCert];
    this.server = tls.createServer(
      {
        key: pair.keyPem,
        cert: pair.certPem,
        ca: [this.pki.ca.certPem],
        // Like Icinga: ask for a certificate, but let an unsigned one in so
        // it can ask for a signed one.
        requestCert: true,
        rejectUnauthorized: false,
      },
      (socket) => this.sessions.push(new Session(socket)),
    );
    await new Promise<void>((resolve) => this.server!.listen(0, "127.0.0.1", resolve));
    return (this.server!.address() as { port: number }).port;
  }

  async close(): Promise<void> {
    for (const s of this.sessions) s.socket.destroy();
    await new Promise<void>((resolve) =>
      this.server ? this.server.close(() => resolve()) : resolve(),
    );
  }

  async waitForSession(match: (s: Session) => boolean, timeoutMs = 30_000): Promise<Session> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const hit = this.sessions.find((s) => !s.closed && match(s));
      if (hit) return hit;
      await sleep(100);
    }
    throw new Error(`no matching agent connection (${this.sessions.length} seen)`);
  }

  /**
   * What Icinga's pki::RequestCertificate handler does: sign the public key
   * the agent connected with. `as` forges what a real master never issues: a
   * certificate for another name, or one signed by a CA that is not ours.
   */
  signPeer(
    session: Session,
    as: { commonName?: string; ca?: { keyPem: string; certPem: string } } = {},
  ): string {
    const caKey = as.ca
      ? (forge.pki.privateKeyFromPem(as.ca.keyPem) as forge.pki.rsa.PrivateKey)
      : this.caKey;
    const caCert = as.ca ? forge.pki.certificateFromPem(as.ca.certPem) : this.caCert;
    const raw = session.socket.getPeerCertificate().raw;
    const peer = forge.pki.certificateFromAsn1(
      forge.asn1.fromDer(forge.util.createBuffer(raw.toString("binary"))),
    );
    const cert = forge.pki.createCertificate();
    cert.publicKey = peer.publicKey;
    // Random, so a renewal yields a certificate that differs from the first.
    cert.serialNumber = "7f" + crypto.randomBytes(8).toString("hex");
    cert.validity.notBefore = new Date();
    cert.validity.notAfter = new Date(Date.now() + 365 * 24 * 3600 * 1000);
    cert.setSubject(
      as.commonName ? [{ name: "commonName", value: as.commonName }] : peer.subject.attributes,
    );
    cert.setIssuer(caCert.subject.attributes);
    cert.setExtensions([
      { name: "basicConstraints", cA: false },
      { name: "keyUsage", digitalSignature: true, keyEncipherment: true },
      { name: "extKeyUsage", serverAuth: true, clientAuth: true },
    ] as unknown as Parameters<typeof cert.setExtensions>[0]);
    cert.sign(caKey, forge.md.sha256.create());
    return forge.pki.certificateToPem(cert);
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

async function waitForLog(nscp: NscpInstance, text: string, timeoutMs = 30_000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (nscp.capturedStdout().includes(text)) return;
    await sleep(200);
  }
  throw new Error(`nscp never logged: ${text}`);
}

describeWithModules("IcingaAgent", "CheckHelpers")(
  "IcingaAgent against a fake Icinga master",
  () => {
    let nscp: NscpInstance;
    let master: FakeIcingaMaster;
    let certDir: string;
    let port: number;

    async function configureAgent(overrides: Record<string, string> = {}): Promise<void> {
      await nscp.configure({
        "/modules": { IcingaAgent: "enabled", CheckHelpers: "enabled" },
        "/settings/log": { level: "debug" },
        "/settings/Icinga/agent": {
          "parent host": "127.0.0.1",
          "parent port": String(port),
          "parent endpoint": PARENT,
          "endpoint name": AGENT,
          ticket: TICKET,
          "certificate directory": certDir,
          ...overrides,
        },
        "/settings/Icinga/agent/commands": {
          "legacy-ok": 'check_ok "message=alias for $who$" $dropped$',
          "fixed-ok": "check_ok message=fixed",
        },
        "/settings/permissions": { enabled: "true" },
        "/settings/permissions/policies": {
          [`IcingaAgent:${PARENT}`]: "CheckHelpers.check_ok, CheckHelpers.check_warning",
        },
      });
    }

    /** The verified connection the agent makes once it holds the CA. */
    async function verifiedSession(): Promise<Session> {
      const session = await master.waitForSession(
        (s) => s.authorized && s.peerCn === AGENT,
        60_000,
      );
      await session.waitFor("icinga::Hello");
      return session;
    }

    beforeAll(async () => {
      nscp = new NscpInstance();
      const pki = generateCertChain({
        outDir: nscp.scratch("master-pki"),
        caCommonName: "Icinga CA",
        signed: {
          master: { commonName: PARENT, isServer: true },
        },
      });
      master = new FakeIcingaMaster(pki);
      port = await master.listen("master");
      certDir = path.join(nscp.workDir, "icinga-certs");
      await configureAgent();
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
      await master?.close();
    });

    let session: Session;

    it("bootstraps: hello and a certificate request carrying the ticket", async () => {
      const first = await master.waitForSession(() => true);
      expect(first.authorized).toBe(false); // self-signed bootstrap certificate
      expect(first.peerCn).toBe(AGENT);
      const hello = await first.waitFor("icinga::Hello");
      expect(hello.capabilities).toBe(2); // IfwApiCheckCommand, not ExecuteArbitraryCommand
      const request = await first.waitFor("pki::RequestCertificate");
      expect(request.ticket).toBe(TICKET);
      session = first;
    });

    it("runs no checks on the connection it could not verify", async () => {
      const cr = await session.execute("early", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(3);
      expect(String(cr.output)).toContain("has not verified this parent");
    });

    it("rejects a signed certificate that is not for its own key", async () => {
      // The master's own certificate: signed by the right CA, wrong key.
      session.send("pki::UpdateCertificate", {
        cert: master.pki.signed.master.certPem,
        ca: master.pki.ca.certPem,
      });
      await waitForLog(nscp, "is not for this agent's key");
      expect(fs.existsSync(path.join(certDir, "ca.crt"))).toBe(false);
      // Nothing was stored, so there is nothing to reconnect with.
      await sleep(500);
      expect(session.closed).toBe(false);
    });

    it("says that the ticket goes to a parent it could not verify", async () => {
      await waitForLog(nscp, "the `ticket` is sent over this unverified connection");
    });

    it("rejects a certificate for its key that carries another name", async () => {
      session.send("pki::UpdateCertificate", {
        cert: master.signPeer(session, { commonName: "some-other-agent" }),
        ca: master.pki.ca.certPem,
      });
      await waitForLog(nscp, "is for 'some-other-agent', not for this agent");
      expect(fs.existsSync(path.join(certDir, "ca.crt"))).toBe(false);
    });

    it("rejects a certificate the CA sent with it did not issue", async () => {
      const other = generateCertChain({
        outDir: nscp.scratch("other-pki"),
        caCommonName: "Icinga CA",
        signed: {},
      });
      session.send("pki::UpdateCertificate", {
        cert: master.signPeer(session, { ca: other.ca }),
        ca: master.pki.ca.certPem,
      });
      await waitForLog(nscp, "was not issued by the Icinga CA");
      expect(fs.existsSync(path.join(certDir, "ca.crt"))).toBe(false);
      await sleep(500);
      expect(session.closed).toBe(false);
    });

    it("stores the signed certificate and reconnects verified", async () => {
      session.send("pki::UpdateCertificate", {
        cert: master.signPeer(session),
        ca: master.pki.ca.certPem,
      });
      const verified = await verifiedSession();
      expect(fs.readFileSync(path.join(certDir, "ca.crt"), "utf8")).toBe(master.pki.ca.certPem);
      // Written beside the targets and renamed into place: nothing left behind.
      expect(fs.readdirSync(certDir).filter((f) => f.endsWith(".new"))).toEqual([]);
      // The key never left the agent; the certificate it now presents is the one we signed.
      expect(fs.existsSync(path.join(certDir, `${AGENT}.key`))).toBe(true);
      if (process.platform !== "win32") {
        expect(fs.statSync(path.join(certDir, `${AGENT}.key`)).mode & 0o077).toBe(0);
      }
      session = verified;
    });

    it("expands ifw-api arguments and returns the check result", async () => {
      const cr = await session.execute("ifw", "nscp-ok", {
        ifw_api_command: "check_ok",
        ifw_api_arguments: { message: { value: "$greeting$ from $who$" } },
        greeting: "hello",
        who: "nscp",
      });
      expect(cr.exit_status).toBe(0);
      expect(cr.output).toBe("hello from nscp");
      expect(cr.check_source).toBe(AGENT);
      expect(Array.isArray(cr.performance_data)).toBe(true);
    });

    it("runs a plain CheckCommand by its name", async () => {
      const cr = await session.execute("plain", "check_warning", {});
      expect(cr.exit_status).toBe(1);
    });

    it("answers an ITL command under its Icinga name", async () => {
      // nscp-local as the Icinga Template Library defines it: the master sends
      // the command's name and the macros its arguments use, nothing else.
      const cr = await session.execute("itl", "nscp-local", {
        nscp_query: "check_ok",
        nscp_arguments: ["message=from the itl"],
        nscp_modules: ["CheckHelpers"],
        nscp_boot: true,
        nscp_showall: false,
        nscp_log_level: "critical",
      });
      expect(cr.exit_status).toBe(0);
      expect(cr.output).toBe("from the itl");
      // The master sees the query that ran, not the name it asked for.
      expect(String(cr.command)).toContain("check_ok");
    });

    it("reports an ITL command it cannot translate instead of guessing", async () => {
      const cr = await session.execute("itl-bad", "disk-windows", { disk_win_warn: "lots" });
      expect(cr.exit_status).toBe(3);
      expect(String(cr.output)).toContain("Cannot translate the Icinga command 'disk-windows'");
    });

    it("runs the translated query under the permission policy, by its own name", async () => {
      // The policy grants check_ok and check_warning; what it is asked about
      // is the query the command became, not "nscp-local".
      const cr = await session.execute("itl-denied", "nscp-local", {
        nscp_query: "check_critical",
      });
      expect(cr.exit_status).toBe(3);
      expect(String(cr.output)).toMatch(/not allowed/i);
      expect(String(cr.output)).toContain("check_critical");
    });

    it("answers a configured command alias with the master's macros", async () => {
      const cr = await session.execute("alias", "legacy-ok", { who: "icinga" });
      expect(cr.exit_status).toBe(0);
      expect(cr.output).toBe("alias for icinga");
    });

    it("runs checks as IcingaAgent:<parent endpoint> under the permission policy", async () => {
      const cr = await session.execute("denied", "check_critical", {
        ifw_api_command: "check_critical",
      });
      expect(cr.exit_status).toBe(3);
      expect(String(cr.output)).toContain(`IcingaAgent:${PARENT}`);
      expect(String(cr.output)).toMatch(/not allowed/i);
    });

    it("refuses event commands", async () => {
      const cr = await session.execute("event", "restart-something", {}, "event_command");
      expect(cr.exit_status).toBe(3);
      expect(String(cr.output)).toContain("Only check commands");
    });

    it("answers a heartbeat with its log position", async () => {
      session.send("event::Heartbeat", { timeout: 120 });
      const position = await session.waitFor("log::SetLogPosition");
      expect(typeof position.log_position).toBe("number");
    });

    it("refuses arguments when allow arguments is false", async () => {
      await nscp.stop();
      await configureAgent({ "allow arguments": "false" });
      nscp.start();
      session = await verifiedSession(); // certificates persist: no bootstrap this time
      const withArgs = await session.execute("args", "nscp-ok", {
        ifw_api_command: "check_ok",
        ifw_api_arguments: { message: { value: "x" } },
      });
      expect(withArgs.exit_status).toBe(3);
      expect(String(withArgs.output)).toContain("arguments are not allowed");
      const without = await session.execute("no-args", "check_ok", { ifw_api_command: "check_ok" });
      expect(without.exit_status).toBe(0);
      // A translated command carries the master's values, so it is refused
      // too; an alias whose arguments are all its own is not.
      const translated = await session.execute("itl-args", "nscp-local", {
        nscp_query: "check_ok",
        nscp_arguments: ["message=x"],
      });
      expect(translated.exit_status).toBe(3);
      expect(String(translated.output)).toContain("arguments are not allowed");
      const aliased = await session.execute("alias-args", "legacy-ok", { who: "icinga" });
      expect(aliased.exit_status).toBe(3);
      const fixed = await session.execute("alias-fixed", "fixed-ok", {});
      expect(fixed.exit_status).toBe(0);
      expect(fixed.output).toBe("fixed");
    });

    it("negotiates the configured tls version and cipher", async () => {
      await nscp.stop();
      await configureAgent({
        "tls version": "1.2",
        "allowed ciphers": "ECDHE-RSA-AES128-SHA256",
      });
      nscp.start();
      const pinned = await verifiedSession();
      expect(pinned.socket.getProtocol()).toBe("TLSv1.2");
      expect(pinned.socket.getCipher().name).toBe("ECDHE-RSA-AES128-SHA256");
      const cr = await pinned.execute("tls", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(0);
    });

    it("asks for a renewed certificate on an open connection and switches to it", async () => {
      await nscp.stop();
      await configureAgent({ "certificate request interval": "2" });
      nscp.start();
      const open = await verifiedSession();
      const certPath = path.join(certDir, `${AGENT}.crt`);
      const before = fs.readFileSync(certPath, "utf8");
      // One request on connect, then another from the timer - on the same
      // connection, without a ticket (the certificate it presents is the proof).
      await open.waitFor("pki::RequestCertificate");
      const requests = () => open.messages.filter((m) => m.method === "pki::RequestCertificate");
      const deadline = Date.now() + 15_000;
      while (requests().length < 2 && Date.now() < deadline) await sleep(200);
      expect(requests().length).toBeGreaterThanOrEqual(2);
      expect(requests()[1].params?.ticket).toBeUndefined();
      expect(open.closed).toBe(false);
      // The master renews, as Icinga does within 30 days of expiry.
      open.send("pki::UpdateCertificate", {
        cert: master.signPeer(open),
        ca: master.pki.ca.certPem,
      });
      const renewed = await master.waitForSession(
        (s) => s !== open && s.authorized && s.peerCn === AGENT,
        30_000,
      );
      await renewed.waitFor("icinga::Hello");
      const after = fs.readFileSync(certPath, "utf8");
      expect(after).not.toBe(before);
      expect(renewed.socket.getPeerCertificate().fingerprint256).toBe(
        new crypto.X509Certificate(after).fingerprint256,
      );
      session = renewed;
      const cr = await session.execute("renewed", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(0);
      // Back to the default for what follows.
      await nscp.stop();
      await configureAgent({ "certificate request interval": "86400" });
      nscp.start();
      session = await verifiedSession();
    });

    it("refuses invalid tls settings at startup instead of on every reconnect", async () => {
      await nscp.stop();
      const before = master.sessions.length;
      await configureAgent({ "tls version": "1.4+", "allowed ciphers": "" });
      nscp.start();
      await waitForLog(nscp, "invalid TLS settings");
      await sleep(1_000);
      expect(master.sessions.length).toBe(before);
      // Back to the defaults for what follows.
      await nscp.stop();
      await configureAgent({ "tls version": "1.2+" });
    });

    it("refuses a parent whose certificate carries another name", async () => {
      await nscp.stop();
      const before = master.sessions.length;
      await configureAgent({ "parent endpoint": "some-other-master" });
      nscp.start();
      await waitForLog(
        nscp,
        "Parent presented the certificate of 'icinga-master', expected 'some-other-master'",
      );
      // It connected (and verified the CA) but never said hello.
      await sleep(1_000);
      const later = master.sessions.slice(before);
      expect(later.length).toBeGreaterThan(0);
      expect(later.some((s) => s.messages.some((m) => m.method === "icinga::Hello"))).toBe(false);
    });
  },
);

describeWithModules("IcingaAgent", "CheckHelpers")(
  "IcingaAgent: trusting the parent on the first connection",
  () => {
    let master: FakeIcingaMaster;
    let port: number;
    let masterFingerprint: string;
    let nscp: NscpInstance | undefined;

    beforeAll(async () => {
      const pki = generateCertChain({
        outDir: fs.mkdtempSync(path.join(os.tmpdir(), "icinga-master-pki-")),
        caCommonName: "Icinga CA",
        signed: {
          master: { commonName: PARENT, isServer: true },
          // What the Icinga 2 agent on the host holds: a certificate this CA
          // signed for the endpoint, and its key.
          agent: { commonName: AGENT },
        },
      });
      master = new FakeIcingaMaster(pki);
      port = await master.listen("master");
      // What `openssl x509 -noout -fingerprint -sha256` prints, colons and all.
      masterFingerprint = new crypto.X509Certificate(pki.signed.master.certPem).fingerprint256;
    });

    afterEach(async () => {
      await nscp?.stop();
      nscp = undefined;
    });

    afterAll(async () => {
      await master?.close();
    });

    /** A fresh agent - no certificates yet - with these settings on top of the basics. */
    async function freshAgent(
      settings: Record<string, string>,
      preinstallCa = false,
      modules: Record<string, string> = {},
    ): Promise<{ agent: NscpInstance; certDir: string }> {
      const agent = new NscpInstance();
      const certDir = path.join(agent.workDir, "icinga-certs");
      if (preinstallCa) {
        fs.mkdirSync(certDir, { recursive: true });
        fs.writeFileSync(path.join(certDir, "ca.crt"), master.pki.ca.certPem);
      }
      await agent.configure({
        "/modules": { IcingaAgent: "enabled", CheckHelpers: "enabled", ...modules },
        "/settings/log": { level: "debug" },
        "/settings/Icinga/agent": {
          "parent host": "127.0.0.1",
          "parent port": String(port),
          "parent endpoint": PARENT,
          "endpoint name": AGENT,
          "certificate directory": certDir,
          ...settings,
        },
      });
      nscp = agent;
      agent.start();
      return { agent, certDir };
    }

    it("runs checks on the first connection when the parent's fingerprint is pinned", async () => {
      const before = master.sessions.length;
      await freshAgent({ "parent fingerprint": masterFingerprint });
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      expect(session.authorized).toBe(false); // the agent still has only its bootstrap certificate
      await session.waitFor("icinga::Hello");
      const cr = await session.execute("pinned", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(0);
    });

    // The translations are unit tested as text. This is the other half: that
    // the queries they name take the arguments they are given, on whatever
    // platform this runs. The macros are the ones the ITL command sends with
    // its defaults; the states depend on the machine, so the test only holds
    // that each one is a real result and not a refused argument.
    itIf(
      moduleBuiltHere("CheckSystem") && moduleBuiltHere("CheckDisk") && moduleBuiltHere("CheckNet"),
    )("runs the ITL's Windows commands against the real checks", async () => {
      const before = master.sessions.length;
      await freshAgent({ "parent fingerprint": masterFingerprint }, false, {
        CheckSystem: "enabled",
        CheckDisk: "enabled",
        CheckNet: "enabled",
      });
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      await session.waitFor("icinga::Hello");

      const commands: Array<[string, string, Json]> = [
        [
          "disk-windows",
          "check_drivesize",
          {
            disk_win_unit: "mb",
            disk_win_warn: "20%",
            disk_win_crit: "100",
            disk_win_path: onWindows ? "C:" : "/",
          },
        ],
        [
          "disk-windows",
          "check_drivesize",
          {
            disk_win_unit: "gb",
            disk_win_warn: "![0-100000]",
            disk_win_show_used: true,
            disk_win_path: onWindows ? "C:" : "/",
          },
        ],
        ["load-windows", "check_cpu", { load_win_warn: 80, load_win_crit: "95%" }],
        [
          "memory-windows",
          "check_memory",
          { memory_win_unit: "mb", memory_win_warn: "10%", memory_win_crit: "64" },
        ],
        [
          "uptime-windows",
          "check_uptime",
          { uptime_win_unit: "h", uptime_win_warn: "[100000-200000]", uptime_win_crit: 200000 },
        ],
        ["procs-windows", "check_process", { procs_win_warn: 100000, procs_win_crit: "!0" }],
        [
          "ping4-windows",
          "check_ping",
          {
            ping_win_address: "127.0.0.1",
            ping_win_packets: "2",
            ping_win_timeout: "1000",
            ping_win_wrta: 500,
            ping_win_wpl: 50,
            ping_win_crta: 900,
            ping_win_cpl: 90,
          },
        ],
        ["network-windows", "check_network", { network_no_isatap: true, network_win_crit: "!-1" }],
      ];
      for (const [command, query, macros] of commands) {
        let cr: Json = {};
        // The collector-backed checks have nothing to report for the first
        // seconds after start.
        for (let attempt = 0; attempt < 30; attempt++) {
          cr = await session.execute(`itl-${command}-${attempt}`, command, macros);
          if (!/collector still initializing/.test(String(cr.output))) break;
          await sleep(1000);
        }
        const what = `${command} -> ${String(cr.command)}: ${String(cr.output)}`;
        expect(String(cr.command)).toContain(query);
        expect([0, 1, 2]).toContain(cr.exit_status);
        expect(what).not.toMatch(/Cannot translate|Invalid|not valid|Failed to/i);
      }
    });

    it("refuses a parent whose certificate does not match the pin", async () => {
      const before = master.sessions.length;
      const { agent } = await freshAgent({ "parent fingerprint": "00".repeat(32) });
      await waitForLog(agent, "not the pinned `parent fingerprint`");
      const later = master.sessions.slice(before);
      expect(later.some((s) => s.messages.some((m) => m.method === "icinga::Hello"))).toBe(false);
    });

    it("verifies the parent from the first connection when ca.crt is installed up front", async () => {
      const before = master.sessions.length;
      const { agent } = await freshAgent({}, true);
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      await session.waitFor("icinga::Hello");
      const cr = await session.execute("preinstalled", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(0);
      expect(agent.capturedStdout()).not.toContain("NOT verified");
    });

    it("connects as the Icinga 2 agent it imported, with no ticket and nothing to sign", async () => {
      // An Icinga 2 agent's data directory: its identity, and a zones.conf
      // that names this master.
      const agent = new NscpInstance();
      const root = agent.scratch("icinga2");
      const certs = path.join(root, "var", "lib", "icinga2", "certs");
      const etc = path.join(root, "etc", "icinga2");
      fs.mkdirSync(certs, { recursive: true });
      fs.mkdirSync(etc, { recursive: true });
      fs.writeFileSync(path.join(certs, `${AGENT}.crt`), master.pki.signed.agent.certPem);
      fs.writeFileSync(path.join(certs, `${AGENT}.key`), master.pki.signed.agent.keyPem);
      fs.writeFileSync(path.join(certs, "ca.crt"), master.pki.ca.certPem);
      fs.writeFileSync(path.join(etc, "constants.conf"), `const NodeName = "${AGENT}"\n`);
      fs.writeFileSync(
        path.join(etc, "zones.conf"),
        `object Endpoint "${PARENT}" {\n  host = "127.0.0.1"\n  port = "${port}"\n}\n` +
          `object Endpoint NodeName {\n}\n`,
      );
      await agent.configure({
        "/modules": { CheckHelpers: "enabled" },
        "/settings/log": { level: "debug" },
      });
      const imported = await agent.run(["icinga-agent", "import", "--from", root]);
      expect(imported.all).toContain(`Imported the Icinga 2 agent '${AGENT}'`);

      const before = master.sessions.length;
      nscp = agent;
      agent.start();
      // The very first connection is the verified one: the master sees the
      // imported certificate, and the agent runs checks on it.
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      expect(session.authorized).toBe(true);
      expect(session.peerCn).toBe(AGENT);
      const hello = await session.waitFor("icinga::Hello");
      expect(hello).toBeDefined();
      const request = await session.waitFor("pki::RequestCertificate");
      expect(request.ticket ?? "").toBe("");
      const cr = await session.execute("imported", "check_ok", { ifw_api_command: "check_ok" });
      expect(cr.exit_status).toBe(0);
      expect(agent.capturedStdout()).not.toContain("NOT verified");
      expect(agent.capturedStdout()).not.toContain("self-signed");
    });

    it("does not connect at all with trust on first use off and nothing to verify against", async () => {
      const before = master.sessions.length;
      const { agent } = await freshAgent({ "trust on first use": "false" });
      await waitForLog(agent, "`trust on first use` is off: not connecting");
      await sleep(1_000);
      expect(master.sessions.length).toBe(before);
    });

    it("keeps a configured endpoint name as written", async () => {
      // Icinga endpoint names and tickets are case-sensitive: only the
      // host-name default is lower-cased, never what the operator wrote.
      const name = "Nscp-Agent-MixedCase";
      const before = master.sessions.length;
      const { certDir } = await freshAgent({ "endpoint name": name });
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      await session.waitFor("icinga::Hello");
      expect(session.peerCn).toBe(name);
      // Read the directory rather than stat the path: Windows and macOS
      // filesystems would find a lower-cased file under this name too.
      expect(fs.readdirSync(certDir)).toContain(`${name}.key`);
    });

    // Unix only: the fixture's stop() is a SIGKILL on Windows - Node has no
    // signal a Windows child turns into a clean shutdown (see
    // rest-session-persistence.test.ts) - so the module's stop() never runs
    // there and there is nothing to observe. The code under test is the same
    // on every platform.
    itOnUnix("closes the connection to the parent when it stops", async () => {
      const before = master.sessions.length;
      const { agent } = await freshAgent({});
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      await session.waitFor("icinga::Hello");
      await agent.stop();
      nscp = undefined;
      // Closed by the agent itself, not by the process going away: a reload
      // stops the agent the same way and must not leave the old socket open.
      expect(agent.capturedStdout()).toContain("Agent is stopping");
      expect(session.closed).toBe(true);
    });

    it("replaces a certificate it cannot use instead of failing every connect", async () => {
      // What a full disk or a kill mid-write used to leave: a truncated
      // certificate beside a key. Both exist, so nothing was regenerated, and
      // TLS could not be set up until someone deleted the file by hand.
      const agent = new NscpInstance();
      const certDir = path.join(agent.workDir, "icinga-certs");
      fs.mkdirSync(certDir, { recursive: true });
      fs.writeFileSync(path.join(certDir, `${AGENT}.crt`), "-----BEGIN CERTIFICATE-----\nMIIB");
      fs.writeFileSync(path.join(certDir, `${AGENT}.key`), "not a key");
      const before = master.sessions.length;
      await agent.configure({
        "/modules": { IcingaAgent: "enabled", CheckHelpers: "enabled" },
        "/settings/Icinga/agent": {
          "parent host": "127.0.0.1",
          "parent port": String(port),
          "parent endpoint": PARENT,
          "endpoint name": AGENT,
          "certificate directory": certDir,
        },
      });
      nscp = agent;
      agent.start();
      await waitForLog(agent, "could not be used together and were replaced");
      const session = await master.waitForSession((s) => master.sessions.indexOf(s) >= before);
      await session.waitFor("pki::RequestCertificate");
      expect(session.peerCn).toBe(AGENT);
    });

    it("refuses a parent fingerprint that is not one", async () => {
      const before = master.sessions.length;
      const { agent } = await freshAgent({ "parent fingerprint": "not-a-fingerprint" });
      await waitForLog(agent, "is not a SHA-256 certificate fingerprint");
      await sleep(1_000);
      expect(master.sessions.length).toBe(before);
    });
  },
);
