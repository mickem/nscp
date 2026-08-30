/**
 * Verifies SyslogClient's stream transports end to end: RFC 6587 syslog over
 * TCP with octet-counted framing, and RFC 5425 syslog over TLS with server
 * certificate verification.
 *
 * Unlike the graphite scenario there is no docker container: a syslog
 * receiver is just a socket that collects bytes, so the "server" is an
 * in-process Node `net` / `tls` server on an ephemeral loopback port (same
 * approach as collectd-submit.test.ts) and the assertions decode the framed
 * records in-process.
 *
 * Covered flows:
 *
 *   1. TCP: a one-shot `nscp syslog --target default ...` submission arrives
 *      as one octet-counted frame ("LEN SP <PRI>TIMESTAMP HOSTNAME TAG MSG").
 *   2. TLS: with the target's `ca` pointing at the CA that signed the server
 *      cert, the default `verify mode = peer` handshake succeeds and the
 *      frame is delivered encrypted.
 *   3. TLS fails closed: with a CA that did NOT sign the server cert the
 *      submission fails and no plaintext reaches the receiver.
 *   4. REST-style argument parsing: the client-query path passes each option
 *      as a single `key=value` token — including the *valued* boolean
 *      `ssl=true` (the po::bool_switch trap, see CLAUDE.md) — and must still
 *      select and complete the TLS transport.
 */
import * as net from "net";
import * as tls from "tls";
import * as fs from "fs";
import { NscpInstance, generateCertChain } from "@fixtures/index";

jest.setTimeout(600_000);

/** Split an RFC 6587 octet-counted stream back into messages. */
function parseOctetCounted(stream: string): string[] {
  const out: string[] = [];
  let pos = 0;
  while (pos < stream.length) {
    const sp = stream.indexOf(" ", pos);
    if (sp < 0) {
      out.push(stream.slice(pos));
      break;
    }
    const len = Number(stream.slice(pos, sp));
    if (!Number.isInteger(len) || len < 0 || sp + 1 + len > stream.length) {
      out.push(stream.slice(pos));
      break;
    }
    out.push(stream.slice(sp + 1, sp + 1 + len));
    pos = sp + 1 + len;
  }
  return out;
}

/**
 * A loopback stream receiver (plain TCP or TLS) that records every byte each
 * connection delivers. TLS handshake failures are counted, not thrown - the
 * fail-closed test asserts on them.
 */
class StreamReceiver {
  private server!: net.Server;
  readonly chunks: Buffer[] = [];
  handshakeFailures = 0;

  constructor(private readonly tlsOptions?: tls.TlsOptions) {}

  async start(): Promise<number> {
    const onSocket = (socket: net.Socket) => {
      socket.on("data", (d: Buffer) => this.chunks.push(d));
      socket.on("error", () => undefined);
    };
    this.server = this.tlsOptions
      ? tls.createServer(this.tlsOptions, onSocket)
      : net.createServer(onSocket);
    this.server.on("tlsClientError", () => {
      this.handshakeFailures += 1;
    });
    await new Promise<void>((resolve, reject) => {
      this.server.once("error", reject);
      this.server.listen(0, "127.0.0.1", () => {
        this.server.off("error", reject);
        resolve();
      });
    });
    return (this.server.address() as net.AddressInfo).port;
  }

  received(): string {
    return Buffer.concat(this.chunks).toString("utf8");
  }

  async waitFor(predicate: (s: string) => boolean, timeoutMs = 30_000): Promise<string> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const s = this.received();
      if (predicate(s)) return s;
      await new Promise((r) => setTimeout(r, 200));
    }
    return this.received();
  }

  async stop(): Promise<void> {
    await new Promise<void>((resolve) => this.server.close(() => resolve()));
  }
}

describe("Syslog stream transports", () => {
  let nscp: NscpInstance;
  let tcpReceiver: StreamReceiver;
  let tlsReceiver: StreamReceiver;
  let tcpPort: number;
  let tlsPort: number;
  let caPath: string;
  let wrongCaPath: string;

  beforeAll(async () => {
    nscp = new NscpInstance();

    // The chain the TLS receiver presents (server cert carries DNS:localhost
    // + IP:127.0.0.1 SANs, so it verifies against an address of 127.0.0.1).
    const certDir = nscp.scratch("syslog_tls");
    const certs = generateCertChain({
      outDir: certDir,
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    caPath = certs.ca.certPath;
    // A second, unrelated CA for the fail-closed test.
    wrongCaPath = generateCertChain({
      outDir: nscp.scratch("syslog_tls_wrong"),
      caCommonName: "wrong-ca",
      signed: { unused: { commonName: "localhost", isServer: true } },
    }).ca.certPath;

    tcpReceiver = new StreamReceiver();
    tcpPort = await tcpReceiver.start();
    tlsReceiver = new StreamReceiver({
      cert: fs.readFileSync(certs.signed.server.certPath),
      key: fs.readFileSync(certs.signed.server.keyPath),
    });
    tlsPort = await tlsReceiver.start();

    await nscp.configure({
      "/modules": { SyslogClient: "enabled" },
      "/settings/syslog/client": {
        // Pin the RFC 3164 HOSTNAME field so the frame assertions are stable
        // regardless of the machine the test runs on.
        hostname: "it-syslog-host",
      },
      "/settings/syslog/client/targets/default": {
        address: `127.0.0.1:${tcpPort}`,
        transport: "tcp",
      },
      "/settings/syslog/client/targets/secure": {
        address: `127.0.0.1:${tlsPort}`,
        transport: "tls",
        // `verify mode` is deliberately NOT set: peer verification must be
        // the default, so only the trust anchor is configured.
        ca: caPath,
        // One attempt: the fail-closed test would otherwise retry the
        // handshake three times before reporting.
        retries: 0,
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
    await tcpReceiver?.stop();
    await tlsReceiver?.stop();
  });

  it("delivers an octet-counted RFC 6587 frame over TCP", async () => {
    const r = await nscp.run([
      "syslog",
      "--target",
      "default",
      "--command",
      "tcp-ok",
      "--result",
      "0",
      "--message",
      "over plain tcp",
    ]);
    expect(r.exitCode).toBe(0);

    const data = await tcpReceiver.waitFor((s) => s.includes("over plain tcp"));
    // Exactly one frame whose length prefix covers the whole record - a
    // framing bug would leave a mangled tail element here.
    const messages = parseOctetCounted(data);
    expect(messages).toHaveLength(1);
    // One well-formed RFC 3164 record: <PRI>TIMESTAMP HOSTNAME TAG MESSAGE.
    // The message may appear twice: the local CLI passes each option as a
    // dashed token plus a redundant undashed pair (see CLAUDE.md), which the
    // exec path turns into two payload lines. That predates the stream
    // transports (UDP datagrams carry the same doubled text) and is not a
    // framing artefact - the REST-style test below submits a single line.
    expect(messages[0]).toMatch(
      /^<\d+>\w{3} [ \d]\d \d\d:\d\d:\d\d it-syslog-host NSCA (over plain tcp)+$/,
    );
  });

  it("delivers over TLS when the CA verifies (verify mode defaults to peer)", async () => {
    const r = await nscp.run([
      "syslog",
      "--target",
      "secure",
      "--command",
      "tls-ok",
      "--result",
      "0",
      "--message",
      "critical over tls",
    ]);
    expect(r.exitCode).toBe(0);

    const data = await tlsReceiver.waitFor((s) => s.includes("critical over tls"));
    const messages = parseOctetCounted(data);
    expect(messages.some((m) => m.includes("critical over tls"))).toBe(true);
  });

  it("fails closed when the server cert is not signed by the trusted CA", async () => {
    const before = tlsReceiver.received().length;
    const r = await nscp.run(
      [
        "syslog",
        "--target",
        "secure",
        // Override the trust anchor with a CA that did not sign the server
        // cert: chain verification must fail and nothing may be delivered.
        "--ca",
        wrongCaPath,
        "--command",
        "tls-bad",
        "--result",
        "0",
        "--message",
        "should not arrive",
      ],
      { allowFailure: true },
    );
    expect(r.exitCode).not.toBe(0);

    // The receiver saw the aborted handshake and no plaintext arrived.
    await tlsReceiver.waitFor(() => tlsReceiver.handshakeFailures > 0, 10_000);
    expect(tlsReceiver.handshakeFailures).toBeGreaterThan(0);
    expect(tlsReceiver.received().length).toBe(before);
    expect(tlsReceiver.received()).not.toContain("should not arrive");
  });

  it("accepts REST-style key=value tokens including the valued boolean ssl=true", async () => {
    // The client-query path passes every option as a single `key=value`
    // token, exactly like a REST-triggered submission. `ssl=true` is the
    // valued-boolean form a po::bool_switch would reject ("does not take any
    // arguments"); it must parse and must select the TLS transport.
    const r = await nscp.run([
      "client",
      "--module",
      "SyslogClient",
      "--boot",
      "--query",
      "submit_syslog",
      `address=127.0.0.1:${tlsPort}`,
      "ssl=true",
      `ca=${caPath}`,
      "retry=0",
      "command=rest-style",
      "result=0",
      "message=valued boolean over tls",
    ]);
    expect(r.exitCode).toBe(0);

    const data = await tlsReceiver.waitFor((s) => s.includes("valued boolean over tls"));
    expect(data).toContain("valued boolean over tls");
  });
});
