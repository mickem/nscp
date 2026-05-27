/**
 * Verifies GraphiteClient's TLS support.
 *
 * Carbon's line receiver is plaintext-only, so "Graphite over TLS" in
 * production means a TLS-terminating proxy in front of carbon. This test
 * stands that proxy up with socat (Dockerfiles/graphite-tls.Dockerfile),
 * points GraphiteClient at it with `ssl = true`, and checks both directions of
 * peer verification:
 *
 *   1. with `verify mode = peer-cert` + the CA that signed the server cert, the
 *      submission is delivered (TLS handshake + hostname verification succeed);
 *   2. with a CA that did NOT sign the server cert, the submission fails and
 *      nothing is delivered (peer verification is actually enforced, i.e. the
 *      client fails closed rather than sending in the clear / skipping checks).
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  containerReadAll,
  dockerOrSkip,
  generateCertChain,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const STATUS_ANCHOR = "nsclient.tls";

dockerOrSkip()("Graphite TLS integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;
  let wrongCaPath: string;

  async function received(): Promise<string> {
    return containerReadAll(server, "/data");
  }
  async function waitForReceived(pred: (s: string) => boolean, timeoutMs = 60_000): Promise<string> {
    const deadline = Date.now() + timeoutMs;
    let last = "";
    while (Date.now() < deadline) {
      last = await received();
      if (pred(last)) return last;
      await new Promise((r) => setTimeout(r, 1000));
    }
    return last;
  }
  const countAnchors = (s: string) => (s.match(/nsclient\.tls/g) ?? []).length;

  beforeAll(async () => {
    nscp = new NscpInstance();

    // The chain that the socat proxy will present (server cert carries
    // DNS:localhost + IP:127.0.0.1 SANs, so it verifies against 127.0.0.1).
    const certDir = nscp.scratch("graphite_tls");
    const certs = generateCertChain({
      outDir: certDir,
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    // A second, unrelated CA for the negative test (did not sign the server cert).
    const wrongDir = nscp.scratch("graphite_tls_wrong");
    wrongCaPath = generateCertChain({
      outDir: wrongDir,
      caCommonName: "wrong-ca",
      signed: { unused: { commonName: "localhost", isServer: true } },
    }).ca.certPath;

    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/graphite-tls.Dockerfile",
    ).build("graphite_tls_receiver", { deleteOnExit: false });

    server = await trackContainerLogs(
      await image
        .withExposedPorts(2003)
        .withBindMounts([{ source: certDir, target: "/certs", mode: "ro" }])
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "graphite_tls_receiver",
    );
    const port = server.getMappedPort(2003);

    await nscp.configure({
      "/modules": { GraphiteClient: "enabled" },
      "/settings/graphite/client/targets/default": {
        address: `127.0.0.1:${port}`,
        ssl: true,
        "verify mode": "peer-cert",
        ca: certs.ca.certPath,
        "status path": "nsclient.tls.${check_alias}.status",
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
    await server?.stop();
  });

  it("delivers a submission over TLS when the CA and hostname verify", async () => {
    const r = await nscp.run([
      "graphite",
      "--target",
      "default",
      "--command",
      "tls-ok",
      "--result",
      "0",
      "--message",
      "over tls",
    ]);
    expect(r.exitCode).toBe(0);
    const data = await waitForReceived((s) => s.includes(STATUS_ANCHOR));
    expect(data).toMatch(/nsclient\.tls\S*\.status 0 \d+/);
  });

  it("fails closed when the server cert is not signed by the trusted CA", async () => {
    const before = countAnchors(await received());
    const r = await nscp.run(
      [
        "graphite",
        "--target",
        "default",
        // Override the target CA with one that did not sign the server cert:
        // chain verification must now fail and the handshake must be rejected.
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
    // The handshake fails before any line is written, so nothing new arrives.
    await new Promise((res) => setTimeout(res, 2000));
    expect(countAnchors(await received())).toBe(before);
  });
});
