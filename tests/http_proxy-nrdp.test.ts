/**
 * Port of tests/http_proxy/run-test.bat (NRDP-via-proxy subset).
 *
 * Topology:
 *   nscp (host)
 *     --> squid proxy   (Docker, ports 3128/3129 published to host)
 *         --> nrdp_origin (Docker, port 80, internal network only)
 *
 * nrdp_origin is intentionally NOT published, so a direct connection
 * from nscp on the host always fails. A successful submission therefore
 * proves the proxy round-trip was used.
 *
 * Tests 5-8 in the bat file modify a boot.ini next to nscp.exe to
 * configure [proxy] for `nscp settings`. That's invasive on a shared
 * binary and the Linux build doesn't even ship a boot.ini by default;
 * those tests are intentionally not ported.
 */
import * as path from "path";
import {
  GenericContainer,
  Network,
  NscpInstance,
  Wait,
  trackContainerLogs,
  type StartedNetwork,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(900_000);

describe("HTTP proxy integration (NRDP)", () => {
  let nscp: NscpInstance;
  let net: StartedNetwork;
  let origin: StartedTestContainer;
  let proxy: StartedTestContainer;
  let openProxyPort: number;
  let authProxyPort: number;

  beforeAll(async () => {
    nscp = new NscpInstance();

    net = await new Network().start();

    const originImage = await GenericContainer.fromDockerfile(
      path.resolve(__dirname, "http_proxy"),
      "Dockerfile.origin",
    ).build("nrdp_origin", { deleteOnExit: false });
    origin = await trackContainerLogs(
      await originImage
        .withNetwork(net)
        .withNetworkAliases("nrdp_origin")
        // No .withExposedPorts: origin must NOT be reachable from the host
        // (proves successful submissions went through the proxy).
        .withWaitStrategy(Wait.forLogMessage(/start worker process|nginx/i))
        .start(),
      "nrdp_origin",
    );

    const proxyImage = await GenericContainer.fromDockerfile(
      path.resolve(__dirname, "http_proxy"),
    ).build("nrdp_proxy", { deleteOnExit: false });
    proxy = await trackContainerLogs(
      await proxyImage
        .withNetwork(net)
        .withExposedPorts(3128, 3129)
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "nrdp_proxy",
    );
    openProxyPort = proxy.getMappedPort(3128);
    authProxyPort = proxy.getMappedPort(3129);
    await new Promise((res) => setTimeout(res, 3000));
  });

  afterAll(async () => {
    await proxy?.stop();
    await origin?.stop();
    await net?.stop();
    await nscp?.stop();
  });

  /**
   * `nscp nrdp` with a fixed payload, varying only the proxy URL the
   * caller provides. The origin hostname inside the docker network is
   * `nrdp_origin` (network alias above) — squid forwards to it.
   */
  async function nrdpThroughProxy(proxyUrl: string, extra: string[] = []) {
    return nscp.run([
      "nrdp",
      "--host", "nrdp_origin",
      "--port", "80",
      "--token", "mytoken",
      "--command", "proxy_test",
      "--message", "HTTP proxy test",
      "--result", "0",
      "--proxy", proxyUrl,
      ...extra,
    ], { allowFailure: true, timeout: 30_000 });
  }

  it("Test 1 - HTTP through open proxy (no auth)", async () => {
    const r = await nrdpThroughProxy(`http://127.0.0.1:${openProxyPort}/`);
    expect(r.exitCode).toBe(0);
  });

  it("Test 2 - HTTP through authenticated proxy (correct credentials)", async () => {
    const r = await nrdpThroughProxy(
      `http://testuser:testpass@127.0.0.1:${authProxyPort}/`,
    );
    expect(r.exitCode).toBe(0);
  });

  it("Test 3 - HTTP through authenticated proxy is rejected with wrong creds", async () => {
    const r = await nrdpThroughProxy(
      `http://testuser:wrongpass@127.0.0.1:${authProxyPort}/`,
    );
    expect(r.exitCode).not.toBe(0);
  });

  it("Test 4 - no-proxy bypass makes origin unreachable", async () => {
    // nrdp_origin is in --no-proxy, so nscp skips the proxy entirely and
    // tries a direct TCP connect. Since the origin has no host-published
    // port, that connect fails — proving the bypass logic actually ran.
    const r = await nrdpThroughProxy(
      `http://127.0.0.1:${openProxyPort}/`,
      ["--no-proxy", "nrdp_origin"],
    );
    expect(r.exitCode).not.toBe(0);
  });
});
