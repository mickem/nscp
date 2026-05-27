/**
 * Brings up a minimal Graphite / carbon line-protocol receiver container and
 * points NSClient++'s GraphiteClient at it, then verifies two flows:
 *
 *   1. System-level metrics — CheckSystem produces system.cpu.* / system.mem.*
 *      metrics; the core metrics scheduler fetches them every `metrics interval`
 *      and hands them to GraphiteClient, which forwards them to the carbon
 *      receiver as "nsclient.<host>.<metric> <value> <ts>" lines.
 *
 *   2. Passive check results — the Scheduler runs `check_ok` (CheckHelpers) on an
 *      interval and reports the result to the GRAPHITE channel; GraphiteClient's
 *      submit handler relays it to the carbon receiver on the target's
 *      configured `status path`.
 *
 * The carbon line protocol is plain TCP, so the "server" is a tiny socket
 * listener (Dockerfiles/graphite.Dockerfile) that appends everything it
 * receives to a file we read back via `docker exec`.
 */
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  containerReadAll,
  dockerOrSkip,
  trackContainerLogs,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

// Distinctive prefix for the check-result status path so the assertion has a
// stable anchor regardless of how the submitted result's `${check_alias}`
// (alias) resolves. Metrics use the separate `metric path`, so this prefix is
// unique to submitted check results.
const STATUS_PATH = "nsclient.itsubmit.${check_alias}.status";
const STATUS_ANCHOR = "nsclient.itsubmit";

dockerOrSkip()("Graphite integration", () => {
  let nscp: NscpInstance;
  let server: StartedTestContainer;

  /** Everything the carbon receiver has captured so far. */
  async function received(): Promise<string> {
    return containerReadAll(server, "/data");
  }

  /** Poll the receiver until `predicate` holds (or the deadline passes). */
  async function waitForReceived(
    predicate: (s: string) => boolean,
    timeoutMs = 60_000,
  ): Promise<string> {
    const deadline = Date.now() + timeoutMs;
    let last = "";
    while (Date.now() < deadline) {
      last = await received();
      if (predicate(last)) return last;
      await new Promise((r) => setTimeout(r, 1000));
    }
    return last; // return whatever we have so the caller's expect() shows a useful diff
  }

  beforeAll(async () => {
    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/graphite.Dockerfile",
    ).build("graphite_receiver", { deleteOnExit: false });

    server = await trackContainerLogs(
      await image
        .withExposedPorts(2003)
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "graphite_receiver",
    );
    const port = server.getMappedPort(2003);

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        // On Linux this build ships libCheckSystem.so (the Unix-aware module,
        // despite the Windows-style name) — there is no separate CheckSystemUnix
        // .so in the build tree, so "CheckSystem" is the correct name here, same
        // as check_mk-agent.test.ts. It is the system-metrics producer
        // (system.cpu.* / system.mem.*).
        CheckSystem: "enabled",
        CheckHelpers: "enabled", // provides check_ok for the scheduled passive check
        Scheduler: "enabled", // runs check_ok and reports it to the GRAPHITE channel
        GraphiteClient: "enabled",
      },
      "/settings/core": {
        // Push metrics frequently so the test doesn't wait the 10s default.
        "metrics interval": "1s",
      },
      "/settings/graphite/client/targets/default": {
        // net::parse splits host:port; connection_data reads host + port from it.
        address: `127.0.0.1:${port}`,
        // Pin a distinctive status path for the check-result assertion (see above).
        "status path": STATUS_PATH,
      },
      // Scheduler-driven passive check: run check_ok every second and report the
      // result to the GRAPHITE channel (GraphiteClient's default channel). The
      // `default` schedule supplies the shared channel/interval/report; the named
      // schedule supplies the command (same shape as check_mk-agent.test.ts).
      "/settings/scheduler/schedules/default": {
        channel: "GRAPHITE",
        interval: "1s",
        report: "all", // include OK results, which is what check_ok returns
      },
      "/settings/scheduler/schedules/graphite_ok": {
        command: "check_ok",
      },
    });

    // Run the full agent so the metrics scheduler is live.
    nscp.start();
  });

  afterAll(async () => {
    await nscp?.stop();
    await server?.stop();
  });

  it("forwards CheckSystem system metrics to Graphite", async () => {
    // CheckSystem always emits cpu metrics on Linux (read from /proc/stat) and
    // memory metrics; the default metric path is "nsclient.${hostname}.${metric}".
    const data = await waitForReceived((s) => /nsclient\.\S*(cpu|mem)/i.test(s));
    // Carbon line protocol: "<dotted.path> <value> <unix-timestamp>".
    expect(data).toMatch(/^nsclient\.\S+ \S+ \d+$/m);
    expect(data).toMatch(/nsclient\.\S*(cpu|mem)/i);
  });

  it("relays a Scheduler-driven check_ok result to Graphite", async () => {
    // No CLI call: the running agent's Scheduler executes check_ok (CheckHelpers)
    // every second and reports the result to the GRAPHITE channel, which
    // GraphiteClient relays to the carbon receiver. Give it a few ticks.
    const data = await waitForReceived((s) => s.includes(STATUS_ANCHOR));
    // send status defaults to true, so the OK (0) result lands on the status
    // path as "<status path> 0 <timestamp>".
    expect(data).toContain(STATUS_ANCHOR);
    expect(data).toMatch(new RegExp(`${STATUS_ANCHOR.replace(/\./g, "\\.")}\\S*\\.status 0 \\d+`));
  });
});
