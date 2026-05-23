/**
 * Port of tests/icinga/run-test.bat. Brings up an Icinga 2 server
 * container, submits passive check results via `nscp icinga`, and
 * verifies each result by calling Icinga's REST API back and grepping
 * the JSON for the expected plugin_output.
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  curlGet,
  trackContainerLogs,
  waitForHttp,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(900_000);

const ICINGA_USER = "nscp";
const ICINGA_PASSWORD = "change_me";

describe("Icinga integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;
  let baseUrl: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname, "icinga"),
    ).build("icinga_server", { deleteOnExit: false });
    server = await trackContainerLogs(
      await image
        .withExposedPorts(5665)
        .withEnvironment({
          ICINGA_API_USER: ICINGA_USER,
          ICINGA_API_PASSWORD: ICINGA_PASSWORD,
        })
        // Icinga 2 takes a beat to come up after the port binds. The bat
        // file polls /v1/status for up to 60s — replicate via a wait
        // strategy below.
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "icinga_server",
    );
    const port = server.getMappedPort(5665);
    baseUrl = `https://127.0.0.1:${port}`;
    await waitForHttp(`${baseUrl}/v1/status`, {
      auth: { user: ICINGA_USER, password: ICINGA_PASSWORD },
      timeoutMs: 60_000,
    });
  });

  afterAll(async () => {
    await server?.stop();
    await nscp?.stop();
  });

  /** GET an Icinga REST object and return the body text. */
  async function fetchObject(typePath: string, name: string): Promise<string> {
    const url = `${baseUrl}/v1/objects/${typePath}/${encodeURIComponent(name)}`;
    return curlGet(url, {
      auth: { user: ICINGA_USER, password: ICINGA_PASSWORD },
    });
  }

  it("Test 1 - basic OK service submission lands plugin_output", async () => {
    const r = await nscp.run([
      "icinga",
      "--address", `${baseUrl}/`,
      "--username", ICINGA_USER, "--password", ICINGA_PASSWORD,
      "--verify", "none",
      "--hostname", "test-host",
      "--command", "basic-ok", "--result", "0", "--message", "all good",
    ]);
    expect(r.exitCode).toBe(0);
    await new Promise((res) => setTimeout(res, 2000));
    const body = await fetchObject("services", "test-host!basic-ok");
    expect(body).toContain("all good");
  });

  it("Test 2 - WARN / CRIT / UNK exit_status propagation", async () => {
    for (const [name, code, msg] of [
      ["code-warn", 1, "warn-message"],
      ["code-crit", 2, "crit-message"],
      ["code-unk",  3, "unk-message"],
    ] as const) {
      const r = await nscp.run([
        "icinga",
        "--address", `${baseUrl}/`,
        "--username", ICINGA_USER, "--password", ICINGA_PASSWORD,
        "--verify", "none",
        "--hostname", "test-host",
        "--command", name, "--result", String(code), "--message", msg,
      ]);
      expect(r.exitCode).toBe(0);
    }
    await new Promise((res) => setTimeout(res, 2500));
    for (const [name, msg] of [
      ["code-warn", "warn-message"],
      ["code-crit", "crit-message"],
      ["code-unk",  "unk-message"],
    ] as const) {
      const body = await fetchObject("services", `test-host!${name}`);
      expect(body).toContain(msg);
    }
  });

  it("Test 3 - host check via the host_check alias", async () => {
    const r = await nscp.run([
      "icinga",
      "--address", `${baseUrl}/`,
      "--username", ICINGA_USER, "--password", ICINGA_PASSWORD,
      "--verify", "none",
      "--hostname", "test-host",
      "--command", "host_check", "--result", "0", "--message", "host alive",
    ]);
    expect(r.exitCode).toBe(0);
    await new Promise((res) => setTimeout(res, 2500));
    const body = await fetchObject("hosts", "test-host");
    expect(body).toContain("host alive");
  });

  it("Test 4 - perfdata round-trip", async () => {
    const r = await nscp.run([
      "icinga",
      "--address", `${baseUrl}/`,
      "--username", ICINGA_USER, "--password", ICINGA_PASSWORD,
      "--verify", "none",
      "--hostname", "test-host",
      "--command", "perf-svc", "--result", "0",
      "--message", "perf-ok|cpu=42 mem=70",
    ]);
    expect(r.exitCode).toBe(0);
    await new Promise((res) => setTimeout(res, 2500));
    const body = await fetchObject("services", "test-host!perf-svc");
    expect(body).toContain("perf-ok");
    expect(body).toContain("cpu");
    expect(body).toContain("mem");
  });

  it("Test 5 - semicolon-in-output round-trip", async () => {
    const r = await nscp.run([
      "icinga",
      "--address", `${baseUrl}/`,
      "--username", ICINGA_USER, "--password", ICINGA_PASSWORD,
      "--verify", "none",
      "--hostname", "test-host",
      "--command", "semi-svc", "--result", "0",
      "--message", "OK; running 3 services; load ok",
    ]);
    expect(r.exitCode).toBe(0);
    await new Promise((res) => setTimeout(res, 2500));
    const body = await fetchObject("services", "test-host!semi-svc");
    expect(body).toContain("running 3 services");
    expect(body).toContain("load ok");
  });

  it("Test 6 - wrong password is rejected", async () => {
    const r = await nscp.run([
      "icinga",
      "--address", `${baseUrl}/`,
      "--username", ICINGA_USER, "--password", "wrong-password",
      "--verify", "none",
      "--hostname", "test-host",
      "--command", "basic-ok", "--result", "0",
      "--message", "should-not-arrive",
    ], { allowFailure: true });
    expect(r.exitCode).not.toBe(0);
    await new Promise((res) => setTimeout(res, 2000));
    const body = await fetchObject("services", "test-host!basic-ok");
    expect(body).not.toContain("should-not-arrive");
  });
});

