/**
 * The NCPA server end-to-end against the REAL check_ncpa.py, fetched at a
 * pinned NCPA release tag and run from a container
 * (Dockerfiles/ncpa.Dockerfile).
 *
 * ncpa-server.test.ts covers the wire format with curl; this one covers
 * the thing that actually matters to a user - that the client Nagios and
 * Nagios XI ship parses what the agent says, exits with the right code,
 * and produces perfdata a frontend can graph.
 *
 * check_ncpa exit codes: 0 OK, 1 WARNING, 2 CRITICAL, 3 UNKNOWN. An
 * `{"error": ...}` body from the agent becomes exit 2 with the message,
 * which is what the bad-token assertion keys on.
 */
import * as path from "path";
import {
  DOCKER_HOST_ALLOWED_HOSTS,
  GenericContainer,
  NscpInstance,
  dockerOrSkip,
  dockerRunOnce,
  generateCertChain,
  hostGatewayExtraHosts,
} from "@fixtures/index";

jest.setTimeout(900_000);

const PORT = 5693;
const TOKEN = "an-ncpa-client-token";
const image = "check_ncpa";

dockerOrSkip()("NCPA server against the stock check_ncpa client", () => {
  let nscp: NscpInstance;

  /** Run check_ncpa against the agent; never throws, the caller asserts. */
  async function checkNcpa(args: string[]) {
    return dockerRunOnce(
      image,
      ["-H", "host.docker.internal", "-P", String(PORT), "-t", TOKEN, ...args],
      { extraHosts: hostGatewayExtraHosts(), allowFailure: true },
    );
  }

  beforeAll(async () => {
    await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/ncpa.Dockerfile",
    ).build(image, { deleteOnExit: false });

    nscp = new NscpInstance();
    const certs = generateCertChain({
      outDir: nscp.scratch("certs"),
      signed: { server: { commonName: "localhost", isServer: true } },
    });
    await nscp.configure({
      "/modules": {
        NCPAServer: "enabled",
        CheckSystem: "enabled",
        CheckDisk: "enabled",
        CheckHelpers: "enabled",
      },
      // The built-in nodes are rendered from the metrics snapshot the core
      // hands consumers every `metrics interval`, which defaults to 10s.
      // Shorten it so a suite does not spend that long waiting for its first
      // sample.
      "/settings/core": { "metrics interval": "1s" },
      "/settings/default": { "allowed hosts": DOCKER_HOST_ALLOWED_HOSTS },
      "/settings/NCPA/server": {
        token: TOKEN,
        certificate: certs.signed.server.certPath,
        "certificate key": certs.signed.server.keyPath,
        // The plugin's -a arguments are the interesting half of the
        // plugins node, so this suite opts into them.
        "allow arguments": "true",
      },
    });
    await nscp.waitForPortFree(PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(PORT, { timeoutMs: 30_000 });
    // Let the 1 Hz collectors publish a snapshot before asserting on values.
    await new Promise((resolve) => setTimeout(resolve, 3000));
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("-l lists the tree as JSON the XI wizard can walk", async () => {
    const r = await checkNcpa(["-l"]);
    expect(r.exitCode).toBe(0);
    const tree = JSON.parse(r.stdout);
    expect(tree).toHaveProperty("root.cpu");
    expect(tree).toHaveProperty("root.memory");
    expect(tree).toHaveProperty("root.plugins");
  });

  it("-M cpu/percent -w 80 -c 90 produces a Nagios line with perfdata", async () => {
    const r = await checkNcpa(["-M", "cpu/percent", "-w", "80", "-c", "90", "-q", "aggregate=avg"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toMatch(/^OK: Percent was /);
    expect(r.stdout).toContain("'percent'=");
    expect(r.stdout).toContain(";80;90;");
  });

  it("-M memory/virtual -u G reports in gigabytes", async () => {
    const r = await checkNcpa(["-M", "memory/virtual", "-u", "G", "-w", "99", "-c", "100"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toMatch(/^OK: Memory usage was /);
    expect(r.stdout).toContain("GB");
  });

  it("a threshold that trips is reported as CRITICAL", async () => {
    const r = await checkNcpa(["-M", "memory/virtual", "-w", "0", "-c", "0"]);
    expect(r.exitCode).toBe(2);
    expect(r.stdout).toMatch(/^CRITICAL: /);
  });

  it("-M disk/logical/<mount> -u G checks a real mount point", async () => {
    // Ask the agent which mount points it has rather than assuming one:
    // the encoded name ("|", "|var|log", "C:|") is platform specific.
    const list = await checkNcpa(["-M", "disk/logical", "-l"]);
    expect(list.exitCode).toBe(0);
    const mounts = Object.keys(JSON.parse(list.stdout).logical ?? {});
    expect(mounts.length).toBeGreaterThan(0);

    const r = await checkNcpa([
      "-M",
      `disk/logical/${mounts[0]}`,
      "-u",
      "G",
      "-w",
      "100",
      "-c",
      "100",
    ]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toMatch(/^OK: Used disk space was /);
  });

  it("-M services -q service=...,status=running checks a service", async () => {
    const list = await checkNcpa(["-M", "services", "-l"]);
    expect(list.exitCode).toBe(0);
    const names = Object.keys(JSON.parse(list.stdout).services ?? {});
    if (names.length === 0) {
      // No init system in this environment. The documented no-data
      // contract is UNKNOWN with a message, not a silent OK.
      const r = await checkNcpa(["-M", "services", "-q", "service=anything,status=running"]);
      expect(r.exitCode).toBe(3);
      expect(r.stdout).toContain("No services found");
      return;
    }
    const r = await checkNcpa(["-M", "services", "-q", `service=${names[0]},status=running`]);
    expect([0, 2]).toContain(r.exitCode);
    expect(r.stdout).toContain(names[0]);
  });

  it("-M processes -q name=... counts matching processes", async () => {
    const r = await checkNcpa(["-M", "processes", "-q", "name=nscp", "-w", "1:", "-c", "1:"]);
    expect(r.stdout).toContain("Process count for processes named nscp");
    expect(r.stdout).toContain("'process_count'=");
  });

  it("-M plugins/<name> runs an agent check and passes its result through", async () => {
    // CheckHelpers' check_ok / check_critical say exactly what they were told
    // to and exit 0 / 2 whatever the state of the host, so this asserts the
    // passthrough rather than the weather.
    const ok = await checkNcpa(["-M", "plugins/check_ok"]);
    expect(ok.exitCode).toBe(0);
    const critical = await checkNcpa(["-M", "plugins/check_critical"]);
    expect(critical.exitCode).toBe(2);

    // A real check, for the perfdata: its verdict depends on the host's
    // uptime, so only the shape is asserted.
    const uptime = await checkNcpa(["-M", "plugins/check_uptime"]);
    expect(uptime.stdout).toMatch(/uptime/i);
    expect(uptime.stdout).toContain("'uptime'=");
  });

  it("-M plugins/... -a passes arguments through as REST-style tokens", async () => {
    // Each -a token is shell-split, percent-encoded and sent as one path
    // segment; the message coming back is what proves it arrived.
    const r = await checkNcpa(["-M", "plugins/check_ok", "-a", "message=hello-from-ncpa"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toContain("hello-from-ncpa");
  });

  it("a wrong token is reported as CRITICAL with the agent's message", async () => {
    const r = await dockerRunOnce(
      image,
      ["-H", "host.docker.internal", "-P", String(PORT), "-t", "wrong-token", "-M", "cpu/percent"],
      { extraHosts: hostGatewayExtraHosts(), allowFailure: true },
    );
    expect(r.exitCode).toBe(2);
    expect(r.stdout).toContain("Incorrect credentials given.");
  });

  it("a node that does not exist is reported as UNKNOWN", async () => {
    const r = await checkNcpa(["-M", "cpu/there-is-no-such-node"]);
    expect(r.exitCode).toBe(3);
    expect(r.stdout).toContain("does not exist");
  });
});
