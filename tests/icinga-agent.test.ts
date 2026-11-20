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
 * parent presenting the wrong name). The end-to-end run against a real
 * Icinga 2.16 master lives in modules/IcingaAgent/spike/.
 *
 * Covered: the ticket exchange on the unverified first connection, the
 * verified reconnect, ifw-api argument expansion, plain CheckCommands, the
 * core permission policy seeing `IcingaAgent:<parent endpoint>`, refused
 * event commands, heartbeats, `allow arguments = false`, and the parent-name
 * pin.
 */
import * as fs from "fs";
import * as path from "path";
import * as tls from "tls";
import * as forge from "node-forge";
import {
  describeWithModules,
  generateCertChain,
  NscpInstance,
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

  /** What Icinga's pki::RequestCertificate handler does: sign the public key the agent connected with. */
  signPeer(session: Session): string {
    const raw = session.socket.getPeerCertificate().raw;
    const peer = forge.pki.certificateFromAsn1(
      forge.asn1.fromDer(forge.util.createBuffer(raw.toString("binary"))),
    );
    const cert = forge.pki.createCertificate();
    cert.publicKey = peer.publicKey;
    cert.serialNumber = "7f";
    cert.validity.notBefore = new Date();
    cert.validity.notAfter = new Date(Date.now() + 365 * 24 * 3600 * 1000);
    cert.setSubject(peer.subject.attributes);
    cert.setIssuer(this.caCert.subject.attributes);
    cert.setExtensions([
      { name: "basicConstraints", cA: false },
      { name: "keyUsage", digitalSignature: true, keyEncipherment: true },
      { name: "extKeyUsage", serverAuth: true, clientAuth: true },
    ] as unknown as Parameters<typeof cert.setExtensions>[0]);
    cert.sign(this.caKey, forge.md.sha256.create());
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

    it("stores the signed certificate and reconnects verified", async () => {
      session.send("pki::UpdateCertificate", {
        cert: master.signPeer(session),
        ca: master.pki.ca.certPem,
      });
      const verified = await verifiedSession();
      expect(fs.readFileSync(path.join(certDir, "ca.crt"), "utf8")).toBe(master.pki.ca.certPem);
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
