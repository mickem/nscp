/**
 * Port of tests/check_mk/run-test-cmk-site.bat — heavy end-to-end test
 * against a real Checkmk site. Pulls the checkmk/check-mk-raw image
 * (~500MB), starts an OMD site, registers our nscp instance as an
 * agent host via the REST API, then runs `cmk -d / -II / -v` to
 * validate the full agent-side pipeline.
 *
 * Slow (~1-2 min site bootstrap + image pull) and depends on Checkmk's
 * upstream image staying available, so it's gated behind an env var
 * and not part of the default `npm test` run. Enable with:
 *   RUN_CMK_SITE_TEST=1 npm test --testPathPatterns cmk-site
 */
import * as path from "path";
import {
  DOCKER_HOST_ALLOWED_HOSTS,
  GenericContainer,
  NscpInstance,
  Wait,
  bundledLuaScript,
  curlGet,
  trackContainerLogs,
  waitForHttp,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(1_800_000); // up to 30 min — site bootstrap is slow

const CHECK_MK_PORT = 6556;

const RUN_CMK_SITE = process.env.RUN_CMK_SITE_TEST === "1";
const SITE = "cmk";
const HOST = "nscp-test";
const CMK_USER = "cmkadmin";
const CMK_PASSWORD = "cmk_e2e_password";
const CMK_IMAGE = process.env.CMK_IMAGE ?? "checkmk/check-mk-raw:latest";

const maybeDescribe = RUN_CMK_SITE ? describe : describe.skip;

maybeDescribe("Checkmk site end-to-end", () => {
  let nscp: NscpInstance;
  let site: StartedTestContainer;
  let baseUrl: string;
  let cmkPort: number;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckMKServer: "enabled",
        LUAScript: "enabled",
        CheckSystemUnix: "enabled",
        CheckHelpers: "enabled",
        CheckDisk: "enabled",
        Scheduler: "enabled",
      },
      "/settings/check_mk/server": {
        port: CHECK_MK_PORT,
        "allowed hosts": DOCKER_HOST_ALLOWED_HOSTS,
      },
      "/settings/check_mk/server/local": { "CPU Load": "command=check_cpu" },
      "/settings/check_mk/server/mrpe":  { Uptime:     "command=check_uptime" },
      "/settings/scheduler/schedules/default": {
        channel: "check_mk-mrpe", interval: "5s", report: "all",
      },
      "/settings/scheduler/schedules/Scheduled_OK": { command: "check_ok" },
      "/settings/scheduler/schedules/Scheduled_Warning": {
        command: "check_always_warning check_ok",
        channel: "check_mk-local",
      },
    });
    await nscp.run(["lua", "add", "--script", bundledLuaScript("mock")]);
    nscp.start();
    await nscp.waitForPort(CHECK_MK_PORT, { timeoutMs: 30_000 });
    // Scheduler-driven passive checks need at least one tick (5s) plus
    // some headroom before their results land in CheckMKServer's
    // submission cache. The bat sleeps 10s before fetching.
    await new Promise((res) => setTimeout(res, 10_000));

    // The Checkmk image is large; pull explicitly so we can retry on
    // intermittent registry failures rather than failing inside
    // testcontainers' less verbose error path.
    site = await trackContainerLogs(
      await new GenericContainer(CMK_IMAGE)
        .withExposedPorts(5000)
        .withEnvironment({
          CMK_SITE_ID: SITE,
          CMK_PASSWORD,
        })
        .withTmpFs({ [`/omd/sites/${SITE}/tmp`]: "rw,exec" })
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "cmk_site",
    );
    cmkPort = site.getMappedPort(5000);
    baseUrl = `http://127.0.0.1:${cmkPort}/${SITE}/check_mk/api/1.0`;

    await waitForHttp(`${baseUrl}/version`, {
      auth: { user: CMK_USER, password: CMK_PASSWORD },
      timeoutMs: 240_000,
    });
  });

  afterAll(async () => {
    await site?.stop();
    if (nscp) {
      try {
        await nscp.run(["nrpe",
          "--host", "127.0.0.1", "--insecure", "--version", "2",
          "--command", "mock_exit"], { timeout: 5_000, allowFailure: true });
      } catch { /* ignore */ }
      await nscp.stop();
    }
  });

  it("registers nscp as a host, runs discovery, and consumes the agent dump", async () => {
    // Create the host via Checkmk REST.
    const createBody = JSON.stringify({
      folder: "/",
      host_name: HOST,
      attributes: {
        ipaddress: "host.docker.internal",
        tag_agent: "cmk-agent",
        tag_address_family: "ip-v4-only",
      },
    });
    await cmkPost(
      `${baseUrl}/domain-types/host_config/collections/all`,
      createBody,
    );

    // Activate changes.
    await cmkPost(
      `${baseUrl}/domain-types/activation_run/actions/activate-changes/invoke`,
      JSON.stringify({
        redirect: false,
        sites: [SITE],
        force_foreign_changes: false,
      }),
      { ifMatch: "*" },
    );
    await new Promise((res) => setTimeout(res, 3_000));

    const agentDump = await cmkExec(`cmk -d ${HOST}`);
    const discovery = await cmkExec(`cmk -II ${HOST}`);
    const check     = await cmkExec(`cmk -v ${HOST}`);

    expect(agentDump).toContain("Version: nsclient++");
    expect(agentDump).toContain("MemTotal:");
    expect(agentDump).toContain("CPU Load");
    expect(agentDump).toContain("Scheduled_OK");
    expect(agentDump).toContain("Scheduled_Warning");
    expect(agentDump).toContain("cached(");

    expect(check).toContain("Filesystem");
    expect(check).toContain("Uptime");
    expect(check).toContain("Service Summary");
    expect(check).toContain("CPU Load");
    expect(check).toContain("Scheduled_OK");
    expect(check).toContain("Scheduled_Warning");

    expect(check).not.toContain("Invalid data:");
    expect(check).not.toContain("UNKNOWN - agent");
  });

  // ----- helpers -----

  async function cmkPost(url: string, body: string, opts: { ifMatch?: string } = {}): Promise<void> {
    // Run curl inside the cmk container to avoid plumbing POST through
    // the shared fixture — it's the only POST in the whole suite.
    const args = ["sh", "-c",
      `curl -sk -f -X POST -u ${CMK_USER}:${CMK_PASSWORD} ` +
      `-H 'Accept: application/json' -H 'Content-Type: application/json' ` +
      (opts.ifMatch ? `-H 'If-Match: ${opts.ifMatch}' ` : "") +
      `'${url}' -d '${body.replace(/'/g, "'\\''")}'`];
    const r = await site.exec(args);
    expect(r.exitCode).toBe(0);
  }

  async function cmkExec(cmd: string): Promise<string> {
    const r = await site.exec(["bash", "-lc", `runuser -u ${SITE} -- ${cmd}`]);
    return r.output;
  }
});

// Silence unused-import warning for curlGet — we re-use waitForHttp only.
void curlGet;
