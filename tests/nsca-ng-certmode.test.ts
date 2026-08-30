/**
 * NSCA-NG certificate-mode (`use psk = false`) TLS tests.
 *
 * The PSK path is covered end-to-end by nsca-ng-submit.test.ts against a real
 * nsca-ng daemon. Cert mode has no docker dependency here: an in-process Node
 * `tls` server speaks the four-line NSCA-NG dialogue (MOIN / PUSH / data /
 * QUIT), which is enough to verify the TLS layer the mode exists for:
 *
 *   1. with `verify mode = peer-cert` + the CA that signed the server cert,
 *      the submission round-trips — and the server REQUIRES a client
 *      certificate, so this also proves the agent presents the configured
 *      certificate/key (mutual TLS);
 *   2. with a CA that did NOT sign the server cert, the handshake fails and
 *      nothing is submitted (peer verification fails closed).
 *
 * Regression context: the module used to configure the SSL context after the
 * TLS stream had been created from it, and SSL_new() copies verify mode,
 * certificate and version bounds out of the context at creation time — so
 * cert mode silently skipped peer verification and never sent the client
 * certificate. Both directions below would have failed before that fix:
 * (1) because the server rejects a client that offers no certificate, and
 * (2) because an unverified client would happily complete the handshake.
 */
import * as net from "net";
import * as tls from "tls";
import { NscpInstance, generateCertChain, type CertBundle } from "@fixtures/index";

jest.setTimeout(120_000);

interface StubServer {
  port: number;
  /** Raw PUSH payloads (external command lines) accepted so far. */
  commands: string[];
  /** TLS-level errors seen (handshake failures land here). */
  tlsErrors: string[];
  close(): Promise<void>;
}

/**
 * Minimal NSCA-NG server on 127.0.0.1: answers MOIN with `MOIN 1`, PUSH with
 * OKAY, records each pushed command block, and OKAYs QUIT. Requires and
 * verifies a client certificate against `clientCa`.
 */
function startStubServer(
  serverCert: { certPem: string; keyPem: string },
  clientCa: string,
): Promise<StubServer> {
  const commands: string[] = [];
  const tlsErrors: string[] = [];
  const server = tls.createServer(
    {
      key: serverCert.keyPem,
      cert: serverCert.certPem,
      ca: [clientCa],
      requestCert: true,
      rejectUnauthorized: true,
    },
    (socket) => {
      let buf = Buffer.alloc(0);
      let pendingData = 0; // bytes of PUSH payload still expected
      socket.on("data", (chunk: Buffer) => {
        buf = Buffer.concat([buf, chunk]);
        for (;;) {
          if (pendingData > 0) {
            if (buf.length < pendingData) return;
            commands.push(buf.subarray(0, pendingData).toString("utf8").trimEnd());
            buf = buf.subarray(pendingData);
            pendingData = 0;
            socket.write("OKAY\n");
            continue;
          }
          const nl = buf.indexOf(0x0a);
          if (nl < 0) return;
          const line = buf.subarray(0, nl).toString("utf8").trimEnd();
          buf = buf.subarray(nl + 1);
          if (line.startsWith("MOIN")) {
            socket.write("MOIN 1\n");
          } else if (line.startsWith("PUSH ")) {
            pendingData = parseInt(line.slice(5), 10);
            socket.write("OKAY\n");
          } else if (line === "QUIT") {
            socket.write("OKAY\n");
            socket.end();
          } else {
            socket.write("BAIL unexpected line\n");
            socket.end();
          }
        }
      });
    },
  );
  server.on("tlsClientError", (err) => {
    tlsErrors.push(String(err?.message ?? err));
  });
  return new Promise((resolve, reject) => {
    server.on("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const port = (server.address() as net.AddressInfo).port;
      resolve({
        port,
        commands,
        tlsErrors,
        close: () => new Promise<void>((res) => server.close(() => res())),
      });
    });
  });
}

describe("NSCA-NG cert mode (TLS peer verification)", () => {
  let nscp: NscpInstance;
  let chain: CertBundle;
  let wrongCaPath: string;
  let server: StubServer;

  beforeAll(async () => {
    nscp = new NscpInstance();

    // Server cert carries DNS:localhost + IP:127.0.0.1 SANs so hostname
    // verification passes against 127.0.0.1; the client cert is signed by the
    // same CA, which the stub server also uses to verify the client.
    chain = generateCertChain({
      outDir: nscp.scratch("nsca_ng_certmode"),
      signed: {
        server: { commonName: "localhost", isServer: true },
        client: { commonName: "nscp-agent" },
      },
    });
    // A second, unrelated CA for the negative test (did not sign the server cert).
    wrongCaPath = generateCertChain({
      outDir: nscp.scratch("nsca_ng_certmode_wrong"),
      caCommonName: "wrong-ca",
      signed: { unused: { commonName: "localhost", isServer: true } },
    }).ca.certPath;

    server = await startStubServer(chain.signed.server, chain.ca.certPem);
  });

  afterAll(async () => {
    await server?.close();
    await nscp?.stop();
  });

  function baseArgs(caPath: string): string[] {
    return [
      "nsca-ng",
      "--host=127.0.0.1",
      `--port=${server.port}`,
      "--no-psk",
      "--verify",
      "peer-cert",
      "--ca",
      caPath,
      "--certificate",
      chain.signed.client.certPath,
      "--certificate-key",
      chain.signed.client.keyPath,
      "--hostname",
      "test-host",
    ];
  }

  it("round-trips a submission over mutually-authenticated TLS", async () => {
    const r = await nscp.run([
      ...baseArgs(chain.ca.certPath),
      "--command",
      "certmode-ok",
      "--result",
      "0",
      "--message",
      "over cert tls",
    ]);
    expect(r.exitCode).toBe(0);
    // The server required and verified a client certificate, so a successful
    // round-trip proves the agent both verified the server and presented the
    // configured client certificate.
    expect(server.commands.join("\n")).toContain(
      "PROCESS_SERVICE_CHECK_RESULT;test-host;certmode-ok;0;over cert tls",
    );
  });

  it("fails closed when the server cert is not signed by the trusted CA", async () => {
    const before = server.commands.length;
    const r = await nscp.run(
      [
        ...baseArgs(wrongCaPath),
        "--command",
        "certmode-bad",
        "--result",
        "0",
        "--message",
        "should not arrive",
      ],
      // Verification failure retries as a network error (attempts with
      // 1s/2s backoff) before giving up, so allow a little extra time.
      { allowFailure: true, timeout: 60_000 },
    );
    expect(r.exitCode).not.toBe(0);
    // The failure must be the TLS handshake itself...
    expect(String(r.all)).toMatch(/handshake/i);
    // ...and nothing may have reached the application protocol.
    expect(server.commands.length).toBe(before);
    expect(server.commands.join("\n")).not.toContain("certmode-bad");
  });

  it("refuses to run unverified without the explicit insecure opt-in", async () => {
    const r = await nscp.run(
      [
        "nsca-ng",
        "--host=127.0.0.1",
        `--port=${server.port}`,
        "--no-psk",
        "--hostname",
        "test-host",
        "--command",
        "certmode-unverified",
        "--result",
        "0",
        "--message",
        "should not arrive",
      ],
      { allowFailure: true, timeout: 60_000 },
    );
    expect(r.exitCode).not.toBe(0);
    expect(String(r.all)).toMatch(/Refusing to connect/i);
    expect(server.commands.join("\n")).not.toContain("certmode-unverified");
  });
});
