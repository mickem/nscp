/**
 * GearmanClient plan, step 5: proxy mode, end to end.
 *
 * Agent mode (gearman-core.test.ts) proves the agent answers the checks of
 * the host it runs on. Proxy mode is the other deployment and the stronger
 * one: one box, typically domain joined, runs the checks of a whole hostgroup
 * through the remote-capable modules that already exist, and the hosts
 * themselves have no agent and no open port at all.
 *
 * So the chain here has one more link than the agent suite's:
 *
 *   Naemon -> NEB module -> gearmand -> NSClient++ (mode = proxy)
 *                                         -> NRPE -> NSClient++ (the target)
 *                        <- check_results <-
 *
 * The core is the Naemon image from step 1 with its proxy object config (see
 * Dockerfiles/entrypoints/gearman-core.sh): a host `nrpe-target` whose every
 * check names its own target through `$HOSTADDRESS$`. Naemon only, the
 * cheaper of the two images - what a second core would exercise is the NEB
 * module's own job text, which is identical in both flavours and already
 * covered by gearman-core.test.ts on both.
 *
 * The NRPE target is a second NSClient++ on the host rather than the
 * `nrpe.Dockerfile` container: that image ships `check_nrpe`, the client, and
 * has no NRPE server to answer. Two agents on one machine is also the more
 * honest shape - the proxy has to reach a *different* agent, and the check it
 * runs (`check_target`, an external script only the target defines) cannot be
 * answered by the proxy itself, so an output that arrives proves the whole
 * chain rather than a shortcut through the proxy's own commands.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  adminStatus,
  bundledSecurityFile,
  dockerOrSkip,
  parseStatusDat,
  trackContainerLogs,
  writeEchoScript,
  type StartedTestContainer,
  type StatusDat,
} from "@fixtures/index";

jest.setTimeout(900_000);

const FIXTURES = path.resolve(__dirname, "..", "modules", "GearmanClient", "fixtures");
const KEY = fs.readFileSync(path.join(FIXTURES, "key.txt"), "utf8");
const HOSTGROUP = "gearman-proxy";
const QUEUE = `hostgroup_${HOSTGROUP}`;
/** The host the entrypoint defines; it has no agent, and the proxy is not it. */
const HOSTNAME = "nrpe-target";
const STATUS_DAT = "/gearman-test/var/status.dat";
const GEARMAN_PORT = 4730;
/** Distinct from the other two gearman suites', so they can never collide. */
const HOST_PORT = 14733;
/** The NRPE target agent, and a port nothing listens on for the `down` case. */
const NRPE_PORT = 15667;
const NRPE_DEAD_PORT = 15999;
/** Only the target agent defines this, which is what makes the output proof. */
const TARGET_OUTPUT = "OK: answered by the nrpe target";

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

async function waitFor<T>(
  produce: () => Promise<T | null>,
  timeoutMs = 120_000,
): Promise<T | null> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const value = await produce();
    if (value !== null) return value;
    if (Date.now() >= deadline) return null;
    await sleep(1000);
  }
}

dockerOrSkip()("a proxy running a whole hostgroup's checks", () => {
  let container: StartedTestContainer;
  /** The agent under test: `mode = proxy`, answering for hosts it is not. */
  let proxy: NscpInstance;
  /** A plain agent behind NRPE, standing in for the monitored host. */
  let target: NscpInstance;
  let scriptsDir: string;
  let startedAt: number;

  async function status(): Promise<StatusDat> {
    try {
      return parseStatusDat((await container.exec(["cat", STATUS_DAT])).output);
    } catch {
      return parseStatusDat("");
    }
  }

  async function serviceResult(
    description: string,
    ok: (fields: Record<string, string>) => boolean,
    timeoutMs = 180_000,
  ): Promise<Record<string, string>> {
    let last: Record<string, string> = {};
    await waitFor(async () => {
      const found = (await status()).services.get(`${HOSTNAME}!${description}`);
      if (!found) return null;
      last = found;
      return Number(found.last_check) >= startedAt && ok(found) ? found : null;
    }, timeoutMs);
    return last;
  }

  async function hostResult(
    ok: (fields: Record<string, string>) => boolean,
    timeoutMs = 180_000,
  ): Promise<Record<string, string>> {
    let last: Record<string, string> = {};
    await waitFor(async () => {
      const found = (await status()).hosts.get(HOSTNAME);
      if (!found) return null;
      last = found;
      return Number(found.last_check) >= startedAt && ok(found) ? found : null;
    }, timeoutMs);
    return last;
  }

  async function waitForRegistration(expected = 2, timeoutMs = 120_000): Promise<number> {
    let last = 0;
    await waitFor(async () => {
      try {
        const found = (await adminStatus("127.0.0.1", HOST_PORT)).get(QUEUE);
        last = found?.workers ?? 0;
        return last >= expected ? found : null;
      } catch {
        return null;
      }
    }, timeoutMs);
    return last;
  }

  beforeAll(async () => {
    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/naemon-gearman.Dockerfile",
    ).build("nscp-it/naemon-gearman", { deleteOnExit: false });
    container = await trackContainerLogs(
      await image
        .withExposedPorts({ container: GEARMAN_PORT, host: HOST_PORT })
        .withEnvironment({
          GEARMAN_KEY: KEY,
          GEARMAN_HOSTGROUP: HOSTGROUP,
          GEARMAN_SCENARIO: "proxy",
          PROXY_TARGET_PORT: String(NRPE_PORT),
          PROXY_DEAD_PORT: String(NRPE_DEAD_PORT),
        })
        .withWaitStrategy(Wait.forLogMessage(/Starting \w+ in the foreground/))
        .withStartupTimeout(180_000)
        .start(),
      "naemon-gearman-proxy",
    );

    // The monitored host: an ordinary agent with an NRPE server and one check
    // of its own. Nothing here knows about gearman. The core runs in a
    // container but both agents run on the host, so the check's script has to
    // be in the host's own flavour.
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-gearman-proxy-"));
    const script = writeEchoScript(scriptsDir, "target", TARGET_OUTPUT);

    target = new NscpInstance();
    await target.configure({
      "/modules": {
        CheckHelpers: "enabled",
        CheckExternalScripts: "enabled",
        NRPEServer: "enabled",
      },
      "/settings/default": { "allowed hosts": "127.0.0.1" },
      "/settings/NRPE/server": {
        port: String(NRPE_PORT),
        // Legacy mode, as in nrpe-tls.test.ts's first phases: what is under
        // test is the proxy reaching another host, not NRPE's TLS.
        insecure: "true",
        // certificate-path is overridden per instance and has no generated
        // DH file, and a missing one aborts the bind.
        dh: bundledSecurityFile("nrpe_dh_2048.pem"),
      },
      "/settings/external scripts": { timeout: "30" },
      "/settings/external scripts/scripts": { check_target: script },
    });
    await target.waitForPortFree(NRPE_PORT);
    target.start();
    await target.waitForPort(NRPE_PORT);

    // The proxy: the same worker as agent mode with the binding off, plus the
    // client module the checks go out through. It has no check_target of its
    // own and no CheckExternalScripts at all.
    proxy = new NscpInstance();
    await proxy.configure({
      "/modules": {
        CheckHelpers: "enabled",
        NRPEClient: "enabled",
        GearmanClient: "enabled",
      },
      "/settings/gearman/worker": {
        server: `127.0.0.1:${HOST_PORT}`,
        key: KEY,
        mode: "proxy",
        hostgroups: HOSTGROUP,
        workers: "2",
      },
    });
    startedAt = Math.floor(Date.now() / 1000);
    proxy.start();
  });

  afterAll(async () => {
    await proxy?.stop();
    await target?.stop();
    await container?.stop();
    if (scriptsDir) fs.rmSync(scriptsDir, { recursive: true, force: true });
  });

  it("registers on the hostgroup queue like any other worker", async () => {
    // Nothing about the registration differs between the modes: the core
    // routes by group and gearmand hands the job to whoever asks first, which
    // is exactly why two proxies on one queue share the load and cover each
    // other with no further configuration.
    expect(await waitForRegistration()).toBe(2);
    expect(proxy.capturedStdout()).toContain("running in proxy mode");
  });

  it("answers a service check for a host it is not, from that host's agent", async () => {
    const remote = await serviceResult("remote", (f) => f.plugin_output.includes("answered"));
    // The output can only have come from the other agent: check_target is an
    // external script the proxy does not define, and CheckExternalScripts is
    // not even loaded there.
    expect(remote.plugin_output).toBe(TARGET_OUTPUT);
    expect(remote.current_state).toBe("0");
    // Active, as far as the core is concerned: it believes it ran the plugin.
    expect(remote.check_type).toBe("0");
  });

  it("answers the host check the same way", async () => {
    // check_version comes back from the target agent itself, so a version
    // string here is the target answering rather than the proxy guessing.
    const host = await hostResult((f) => /\d+\.\d+\.\d+/.test(f.plugin_output));
    expect(host.plugin_output).toMatch(/^\d+\.\d+\.\d+/);
    expect(host.current_state).toBe("0");
    expect(host.check_type).toBe("0");
  });

  it("reports a target it cannot reach instead of going quiet", async () => {
    // The failure mode that matters in proxy mode: the host is down, not the
    // proxy. The core has to hear about it from the proxy - silence would be
    // an orphaned check, which reads as the whole proxy being gone.
    const down = await serviceResult("down", (f) => f.plugin_output.length > 0);
    expect(down.plugin_output).toContain(`127.0.0.1:${NRPE_DEAD_PORT}`);
    expect(down.current_state).toBe("3");
    expect(down.check_type).toBe("0");
  });

  it("keeps the proxy's own name out of the results", async () => {
    // Every result is filed under the host the core scheduled the check for.
    // A proxy that answered under its own name would collapse a whole
    // hostgroup onto one host in the core.
    const dat = await status();
    for (const key of dat.services.keys()) expect(key.startsWith(`${HOSTNAME}!`)).toBe(true);
    expect([...dat.hosts.keys()]).toEqual([HOSTNAME]);
  });
});
