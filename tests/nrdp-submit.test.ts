/**
 * Port of tests/nrdp/run-test.bat. Brings up the nrdp_server container,
 * pushes passive check results through `nscp nrdp` over HTTP and HTTPS,
 * and verifies each result landed in the container's bind-mounted spool
 * directory.
 */
import * as os from "os";
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  containerChmodReadable,
  anyFileContains,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const TOKEN = "change_me";

describe("NRDP integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;
  let spoolDir: string;
  let httpPort: number;
  let httpsPort: number;

  beforeAll(async () => {
    nscp = new NscpInstance();
    spoolDir = nscp.scratch("nrdp_test");

    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/nrdp.Dockerfile",
    ).build("nrdp_server", { deleteOnExit: false });

    server = await trackContainerLogs(
      await image
        .withExposedPorts(80, 443)
        .withEnvironment({ TOKEN })
        .withBindMounts([
          { source: spoolDir, target: "/nrdp/checkresults", mode: "rw" },
        ])
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "nrdp_server",
    );

    httpPort = server.getMappedPort(80);
    httpsPort = server.getMappedPort(443);
  });

  afterAll(async () => {
    await nscp?.stop();
    await server?.stop();
  });

  async function submit(
    name: string,
    code: number,
    message: string,
    address: string,
  ): Promise<void> {
    const r = await nscp.run([
      "nrdp",
      "--address", address,
      "--verify=none",
      "--token", TOKEN,
      "--command", name,
      "--result", String(code),
      "--message", message,
    ], { allowFailure: true });
    if (r.exitCode !== 0) {
      // eslint-disable-next-line no-console
      console.error(`nscp nrdp failed (${r.exitCode}):\n${r.all ?? r.stdout ?? r.stderr}`);
    }
    expect(r.exitCode).toBe(0);
    // PHP under Apache writes spool files as www-data with mode 0660;
    // chmod inside the container so the host-side test process can read
    // them via the bind mount.
    await containerChmodReadable(server, "/nrdp/checkresults");
    expect(anyFileContains(spoolDir, name)).toBe(true);
    expect(anyFileContains(spoolDir, message)).toBe(true);
  }

  it.each([
    ["ok-check",       0, "Everything is fine"],
    ["warning-check",  1, "Slightly worried"],
    ["critical-check", 2, "Houston we have a problem"],
    ["unknown-check",  3, "No idea"],
  ])("submits %s (HTTP, result=%i)", async (name, code, msg) => {
    await submit(name, code, msg, `http://127.0.0.1:${httpPort}/nrdp/server/`);
  });

  it.each([
    ["ok-check-https",       0, "Everything is fine over TLS"],
    ["warning-check-https",  1, "Slightly worried over TLS"],
    ["critical-check-https", 2, "Houston we have a TLS problem"],
    ["unknown-check-https",  3, "No idea over TLS"],
  ])("submits %s (HTTPS, result=%i)", async (name, code, msg) => {
    await submit(name, code, msg, `https://127.0.0.1:${httpsPort}/nrdp/server/`);
  });

  it("expands hostname template macros end-to-end", async () => {
    // NRDPClient splits boost::asio::ip::host_name() on the first '.' into
    // host / domain and substitutes ${host}, ${host_lc}, ${host_uc},
    // ${domain}, ${domain_lc}, ${domain_uc} into the configured hostname.
    const hostnameFull = os.hostname();
    const dot = hostnameFull.indexOf(".");
    const hostPart = dot >= 0 ? hostnameFull.substring(0, dot) : hostnameFull;
    const domainPart = dot >= 0 ? hostnameFull.substring(dot + 1) : "";
    const template =
      "h-${host}-hl-${host_lc}-hu-${host_uc}-d-${domain}-dl-${domain_lc}-du-${domain_uc}";
    const expected =
      `h-${hostPart}-hl-${hostPart.toLowerCase()}-hu-${hostPart.toUpperCase()}` +
      `-d-${domainPart}-dl-${domainPart.toLowerCase()}-du-${domainPart.toUpperCase()}`;

    const r = await nscp.run([
      "nrdp",
      "--define", `/settings/NRDP/client:hostname=${template}`,
      "--address", `http://127.0.0.1:${httpPort}/nrdp/server/`,
      "--token", TOKEN,
      "--command", "macro-check",
      "--result", "0",
      "--message", "Macro hostname check",
    ]);
    expect(r.exitCode).toBe(0);
    await containerChmodReadable(server, "/nrdp/checkresults");
    expect(anyFileContains(spoolDir, "macro-check")).toBe(true);
    expect(anyFileContains(spoolDir, expected)).toBe(true);
    // Make sure none of the macro tokens leaked through unexpanded.
    expect(anyFileContains(spoolDir, "${host}")).toBe(false);
    expect(anyFileContains(spoolDir, "${domain}")).toBe(false);
  });

  it("rejects an invalid token over HTTP", async () => {
    const r = await nscp.run([
      "nrdp",
      "--address", `http://127.0.0.1:${httpPort}/nrdp/server/`,
      "--token=wrong_token",
      "--command", "bad-token-check",
      "--result", "0",
      "--message", "should not be accepted",
    ], { allowFailure: true });
    expect(r.exitCode).not.toBe(0);
    await containerChmodReadable(server, "/nrdp/checkresults");
    expect(anyFileContains(spoolDir, "bad-token-check")).toBe(false);
  });

  it("rejects an invalid token over HTTPS", async () => {
    const r = await nscp.run([
      "nrdp",
      "--address", `https://127.0.0.1:${httpsPort}/nrdp/server/`,
      "--verify=none",
      "--token=wrong_token",
      "--command", "bad-token-check-https",
      "--result", "0",
      "--message", "should not be accepted over TLS",
    ], { allowFailure: true });
    expect(r.exitCode).not.toBe(0);
    await containerChmodReadable(server, "/nrdp/checkresults");
    expect(anyFileContains(spoolDir, "bad-token-check-https")).toBe(false);
  });
});
