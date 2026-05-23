/**
 * Port of tests/check_mk/run-test.bat. Configures `nscp test` as a
 * Check_MK agent (CheckMKServer + local/MRPE entries + scheduler-driven
 * passive checks on a 5-second tick), pulls the agent dump from a tiny
 * netcat container, and asserts on the body of the dump.
 *
 * The bat enables the Windows-only CheckSystem module; this port uses
 * the Linux-compatible names where they differ (the build wires
 * CheckSystemUnix on Linux).
 */
import * as path from "path";
import {
  DOCKER_HOST_ALLOWED_HOSTS,
  GenericContainer,
  NscpInstance,
  bundledLuaScript,
  dockerOrSkip,
  dockerRunOnce,
  hostGatewayExtraHosts,
} from "@fixtures/index";

jest.setTimeout(900_000);

const CHECK_MK_PORT = 6556;

dockerOrSkip()("check_mk integration", () => {
  let nscp: NscpInstance;
  let image: string;
  let agentDump = "";

  beforeAll(async () => {
    image = "check_mk_client";
    await GenericContainer.fromDockerfile(path.resolve(__dirname), "Dockerfiles/check_mk.Dockerfile").build(
      image,
      { deleteOnExit: false },
    );

    nscp = new NscpInstance();

    await nscp.configure({
      "/modules": {
        CheckMKServer: "enabled",
        LUAScript: "enabled",
        // The bat enables `CheckSystem` and `CheckDisk` (Windows
        // module names). On Linux this build ships only one of them:
        // libCheckSystem.so is the Unix-aware module despite the
        // Windows-style name — there is no separate CheckSystemUnix
        // .so on disk, and CheckDisk isn't built on Linux at all.
        // Without a real .so the activation logs an error and the
        // module is silently absent, which is why the agent dump
        // came back empty (no <<<mem>>>, <<<df>>>, etc.).
        CheckSystem: "enabled",
        CheckHelpers: "enabled",
        Scheduler: "enabled",
      },
      "/settings/check_mk/server": {
        port: CHECK_MK_PORT,
        "allowed hosts": DOCKER_HOST_ALLOWED_HOSTS,
      },
      "/settings/check_mk/server/local": { "CPU Load": "command=check_cpu" },
      "/settings/check_mk/server/mrpe":  { Uptime:     "command=check_uptime" },
      "/settings/scheduler/schedules/default": {
        channel:  "check_mk-mrpe",
        interval: "5s",
        report:   "all",
      },
      "/settings/scheduler/schedules/Scheduled_OK": {
        command: "check_ok",
      },
      "/settings/scheduler/schedules/Scheduled_Warning": {
        command: "check_always_warning check_ok",
        channel: "check_mk-local",
      },
    });
    // `mock.lua` provides the NRPE mock_query / mock_exit commands.
    // `default_check_mk.lua` is auto-loaded by CheckMKServer's own
    // script manager when its [/settings/check_mk/server/scripts]
    // section is empty (see CheckMKServer.cpp:98), so we don't register
    // it via LUAScript here.
    await nscp.run(["lua", "add", "--script", bundledLuaScript("mock")]);

    nscp.start();
    await nscp.waitForPort(CHECK_MK_PORT, { timeoutMs: 30_000 });
    // Scheduler-driven passive checks need at least one tick (5s) plus
    // some headroom before their results land in CheckMKServer's
    // submission cache. The bat sleeps 10s before fetching.
    await new Promise((res) => setTimeout(res, 10_000));

    const r = await dockerRunOnce(image, [
      "host.docker.internal", String(CHECK_MK_PORT),
    ], { extraHosts: hostGatewayExtraHosts(), allowFailure: true });
    agentDump = r.all ?? r.stdout ?? "";
  });

  afterAll(async () => {
    if (nscp) {
      try {
        await nscp.run(["nrpe",
          "--host", "127.0.0.1", "--insecure", "--version", "2",
          "--command", "mock_exit"], { timeout: 5_000, allowFailure: true });
      } catch { /* ignore */ }
      await nscp.stop();
    }
  });

  it("agent dump is non-empty", () => {
    if (agentDump.length === 0) {
      // eslint-disable-next-line no-console
      console.error("Empty agent dump — likely allowed-hosts blocked the bridge IP or CheckMKServer never bound 6556");
    }
    expect(agentDump.length).toBeGreaterThan(0);
  });

  it.each([
    "Version: nsclient++",
    "AgentOS:",
    "Hostname:",
    "MemTotal:",
    "CPU Load",
    "Uptime ",
    "Scheduled_OK",
    "Scheduled_Warning",
    "cached(",
  ])("dump contains %s", (needle) => {
    expect(agentDump).toContain(needle);
  });
});
