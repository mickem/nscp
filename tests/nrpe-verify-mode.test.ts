/**
 * The `verify mode` a listener is configured with, and what happens when it is
 * not one the parser knows.
 *
 * The parser used to accept five spellings and silently drop everything else.
 * `fail-if-no-peer-cert` was not one of the five - but it is the spelling the
 * permissions guide, the NRPE scenario walkthrough and the `client identity
 * source` help text all tell operators to write. So
 * `verify mode = peer,fail-if-no-peer-cert` resolved to bare `verify_peer`:
 * the listener asked the client for a certificate and completed the handshake
 * when none arrived. An operator whose configuration matched the documentation
 * was running an NRPE listener authenticated by the allowed-hosts IP list.
 *
 * No docker: both cases are observable from the agent's own NRPE client.
 */
import {
  NscpInstance,
  bundledSecurityFile,
  generateCertChain,
} from "@fixtures/index";

jest.setTimeout(300_000);

describe("NRPE verify mode", () => {
  let nscp: NscpInstance;
  let certs: ReturnType<typeof generateCertChain>;

  /** Configure the NRPE listener with `verifyMode` and try to start it. */
  async function startWith(verifyMode: string): Promise<void> {
    await nscp.configure({
      // CheckHelpers supplies check_ok, the command the calls below run: this
      // suite is about whether the handshake completes, so the check itself has
      // to be one that always answers.
      "/modules": { NRPEServer: "enabled", CheckHelpers: "enabled" },
      "/settings/NRPE/server": {
        "allowed hosts": "127.0.0.1",
        "use ssl": true,
        insecure: false,
        "verify mode": verifyMode,
        dh: bundledSecurityFile("nrpe_dh_2048.pem"),
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
        ca: certs.ca.certPath,
      },
    });
    nscp.start();
  }

  beforeEach(() => {
    nscp = new NscpInstance();
    certs = generateCertChain({
      outDir: nscp.scratch("nrpe-verify-mode"),
      signed: {
        server: { commonName: "localhost", isServer: true },
        client: { commonName: "localhost" },
      },
    });
  });

  afterEach(async () => {
    await nscp?.stop();
  });

  it("requires a client certificate when configured with the documented spelling", async () => {
    await startWith("peer,fail-if-no-peer-cert");
    await nscp.waitForPort(5666, { timeoutMs: 30_000 });

    // With a client certificate the call goes through.
    const withCert = await nscp.run(
      [
        "nrpe",
        "--host",
        "127.0.0.1",
        "--certificate",
        certs.signed.client.certPath,
        "--certificate-key",
        certs.signed.client.keyPath,
        "--command",
        "check_ok",
      ],
      { allowFailure: true },
    );
    expect(withCert.exitCode).toBe(0);

    // Without one the handshake must fail. This is the regression: the token
    // used to be dropped, the listener asked for a certificate and then
    // accepted its absence.
    const withoutCert = await nscp.run(["nrpe", "--host", "127.0.0.1", "--command", "check_ok"], {
      allowFailure: true,
    });
    expect(withoutCert.exitCode).not.toBe(0);
  });

  it("refuses to listen when verify mode holds a token it does not know", async () => {
    // Fail closed: a typo used to degrade the listener in the direction of
    // accepting more, which is the worst possible direction for it to fail in.
    await startWith("peer,fail-if-no-peer-cet");

    await expect(nscp.waitForPort(5666, { timeoutMs: 15_000 })).rejects.toThrow();
    const log = nscp.capturedStdout() + nscp.capturedStderr();
    expect(log).toMatch(/fail-if-no-peer-cet/);
  });
});
