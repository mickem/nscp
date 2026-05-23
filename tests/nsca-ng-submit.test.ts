/**
 * Port of tests/nsca-ng/run-test.bat. Brings up the nsca-ng server with a
 * fixed PSK, pushes a series of submissions through `nscp nsca-ng`, then
 * verifies each one landed (or didn't, for negative tests) in the
 * bind-mounted results.txt.
 */
import * as fs from "fs";
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  anyFileContains,
  containerChmodReadable,
  dockerOrSkip,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const IDENTITY = "nscp";
const PASSWORD = "change_me";

dockerOrSkip()("NSCA-NG integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;
  let spoolDir: string;
  let resultsFile: string;
  let port: number;

  beforeAll(async () => {
    nscp = new NscpInstance();
    spoolDir = nscp.scratch("nsca_ng_test");
    resultsFile = path.join(spoolDir, "results.txt");
    // Pre-create as a file so docker bind-mounts it as a file rather
    // than as a directory if the daemon createMode is racy.
    fs.writeFileSync(resultsFile, "");

    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/nsca-ng.Dockerfile",
    ).build("nsca_ng_server", { deleteOnExit: false });
    server = await trackContainerLogs(
      await image
        .withExposedPorts(5668)
        .withEnvironment({
          NSCA_NG_IDENTITY: IDENTITY,
          NSCA_NG_PASSWORD: PASSWORD,
        })
        .withBindMounts([{ source: spoolDir, target: "/nsca-ng", mode: "rw" }])
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "nsca_ng_server",
    );
    port = server.getMappedPort(5668);
    // Allow the daemon another moment after the port is open — the bat
    // file polls TCP up to 30 times before submitting for the same reason.
    await new Promise((res) => setTimeout(res, 1500));
  });

  afterAll(async () => {
    await server?.stop();
    await nscp?.stop();
  });

  async function expectInResults(needle: string): Promise<void> {
    // Give the daemon a moment to flush.
    await containerChmodReadable(server, "/nsca-ng");
    await new Promise((r) => setTimeout(r, 500));
    expect(anyFileContains(spoolDir, needle)).toBe(true);
  }

  async function expectNotInResults(needle: string): Promise<void> {
    await containerChmodReadable(server, "/nsca-ng");
    await new Promise((r) => setTimeout(r, 500));
    expect(anyFileContains(spoolDir, needle)).toBe(false);
  }

  it("submits a basic OK service result", async () => {
    const r = await nscp.run([
      "nsca-ng",
      "--host=127.0.0.1",
      `--port=${port}`,
      `--identity=${IDENTITY}`,
      `--password=${PASSWORD}`,
      "--hostname=test-host",
      "--command",
      "basic-ok",
      "--result",
      "0",
      "--message",
      "all good",
    ]);
    expect(r.exitCode).toBe(0);
    await expectInResults("PROCESS_SERVICE_CHECK_RESULT;test-host;basic-ok;0;all good");
  });

  it("encodes WARN / CRIT / UNKNOWN as 1 / 2 / 3", async () => {
    for (const [name, code, msg] of [
      ["code-warn", 1, "warn"],
      ["code-crit", 2, "crit"],
      ["code-unk", 3, "unk"],
    ] as const) {
      const r = await nscp.run([
        "nsca-ng",
        "--host=127.0.0.1",
        `--port=${port}`,
        `--identity=${IDENTITY}`,
        `--password=${PASSWORD}`,
        "--hostname=test-host",
        "--command",
        name,
        "--result",
        String(code),
        "--message",
        msg,
      ]);
      expect(r.exitCode).toBe(0);
      await expectInResults(`PROCESS_SERVICE_CHECK_RESULT;test-host;${name};${code};${msg}`);
    }
  });

  it("routes --host-check to PROCESS_HOST_CHECK_RESULT", async () => {
    const r = await nscp.run([
      "nsca-ng",
      "--host=127.0.0.1",
      `--port=${port}`,
      `--identity=${IDENTITY}`,
      `--password=${PASSWORD}`,
      "--hostname=test-host",
      "--host-check",
      "--command",
      "ignored-when-host-check",
      "--result",
      "0",
      "--message",
      "host alive",
    ]);
    expect(r.exitCode).toBe(0);
    await expectInResults("PROCESS_HOST_CHECK_RESULT;test-host;0;host alive");
    await expectNotInResults("PROCESS_SERVICE_CHECK_RESULT;test-host;ignored-when-host-check");
  });

  it("preserves semicolons in plugin output round-trip (B1 regression)", async () => {
    const r = await nscp.run([
      "nsca-ng",
      "--host=127.0.0.1",
      `--port=${port}`,
      `--identity=${IDENTITY}`,
      `--password=${PASSWORD}`,
      "--hostname=test-host",
      "--command",
      "semicolon-test",
      "--result",
      "0",
      "--message",
      "OK; running 3 services; load ok",
    ]);
    expect(r.exitCode).toBe(0);
    // See the long comment in the bat file. The wire form is
    // "OK\; running 3 services\; load ok" and we assert in two parts to
    // dodge the backslash-escape brittleness rather than chasing the
    // exact escaped form.
    await expectInResults("PROCESS_SERVICE_CHECK_RESULT;test-host;semicolon-test;0;OK");
    await expectInResults("load ok");
    // And confirm the result wasn't split into a second command line by
    // a missed semicolon escape.
    await expectNotInResults("PROCESS_SERVICE_CHECK_RESULT;running");
  });

  it("rejects a wrong password (negative test)", async () => {
    const r = await nscp.run(
      [
        "nsca-ng",
        "--host=127.0.0.1",
        `--port=${port}`,
        `--identity=${IDENTITY}`,
        "--password=wrong-password",
        "--hostname=test-host",
        "--command",
        "should-not-arrive",
        "--result",
        "0",
        "--message",
        "nope",
      ],
      { allowFailure: true },
    );
    expect(r.exitCode).not.toBe(0);
    await expectNotInResults("should-not-arrive");
  });
});
