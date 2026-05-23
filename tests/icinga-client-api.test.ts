/**
 * Port of tests/icinga-client/run-test.bat. Builds Icinga's
 * check_nscp_api in a docker container (see icinga-client.Dockerfile)
 * and runs it against a locally-started `nscp test` instance. The
 * host's nscp binds 8443 for the WEB server; the container reaches it
 * via host.docker.internal.
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  bundledLuaScript,
  dockerRunOnce,
  generateCertChain,
  hostGatewayExtraHosts,
} from "@fixtures/index";

jest.setTimeout(900_000);

const PASSWORD = "test-password";
const NSCP_PORT = 8443;

describe("Icinga client (check_nscp_api) integration", () => {
  let nscp: NscpInstance;
  let image: string;

  beforeAll(async () => {
    // Default to the icinga2-bin Debian install path ("~seconds to build");
    // honour the DOCKERFILE env override for the from-source path. The env
    // var takes a name relative to tests/Dockerfiles/, e.g.
    // `DOCKERFILE=icinga-client-source.Dockerfile`.
    const dockerfile = `Dockerfiles/${process.env.DOCKERFILE ?? "icinga-client.Dockerfile"}`;
    image = "check_nscp_api";
    await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      dockerfile,
    ).build(image, { deleteOnExit: false });

    nscp = new NscpInstance();

    // The Linux build does not ship a default WEBServer certificate at
    // ${certificate-path}/certificate.pem, and WEBServer dereferences
    // that path unconditionally on boot (boost::optional assertion).
    // Generate a throwaway cert and point WEBServer at it explicitly.
    const certsDir = nscp.scratch("certs");
    const certs = generateCertChain({
      outDir: certsDir,
      signed: { server: { commonName: "localhost", isServer: true } },
    });

    await nscp.run([
      "web", "install",
      "--password", PASSWORD,
      "--allowed-hosts", "127.0.0.1,0.0.0.0/0",
    ]);
    // `nscp web install`'s --certificate / --certificate-key flags don't
    // actually persist into the INI, so the WEBServer module falls back
    // to the (non-existent) default cert path and crashes on boot. Pin
    // the cert explicitly via the settings path. Same for the matching
    // private key.
    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled",
        LUAScript: "enabled",
      },
      "/settings/WEB/server": {
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
      },
    });
    await nscp.run(["lua", "add", "--script", bundledLuaScript("mock")]);
    // The original bat doesn't install NRPE here, but a few suites that
    // share this fixture pattern use the NRPE mock_exit command as a
    // clean-shutdown channel for `nscp test`. Keep NRPE off here so the
    // /settings/default allowed-hosts written by `web install` (which
    // includes the docker bridge range) isn't overwritten by NRPE's
    // 127.0.0.1-only install.
    nscp.start();
    await nscp.waitForPort(NSCP_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("Test 1 - valid password gets the mock_query response", async () => {
    const r = await dockerRunOnce(image, [
      "--host", "host.docker.internal",
      "--port", String(NSCP_PORT),
      "--password", PASSWORD,
      "--query", "mock_query",
    ], { extraHosts: hostGatewayExtraHosts(), allowFailure: true });
    expect(r.exitCode).toBe(0);
    expect(r.all).toContain("mock_query::");
  });

  it("Test 2 - invalid password is rejected with a non-zero status", async () => {
    const r = await dockerRunOnce(image, [
      "--host", "host.docker.internal",
      "--port", String(NSCP_PORT),
      "--password", "definitely-not-the-password",
      "--query", "mock_query",
    ], { extraHosts: hostGatewayExtraHosts(), allowFailure: true });
    // check_nscp_api maps auth failure to UNKNOWN (3) or CRITICAL (2);
    // any non-zero exit code is acceptable.
    expect(r.exitCode).not.toBe(0);
  });

  it("Test 3 - WARNING propagates with the message body", async () => {
    const r = await dockerRunOnce(image, [
      "--host", "host.docker.internal",
      "--port", String(NSCP_PORT),
      "--password", PASSWORD,
      "--query", "check_always_warning",
      "--arguments", "check_ok", "message=warn-from-icinga",
    ], { extraHosts: hostGatewayExtraHosts(), allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.all).toContain("warn-from-icinga");
  });
});
