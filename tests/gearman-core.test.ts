/**
 * GearmanClient plan, step 4: the agent answering real monitoring cores.
 *
 * Steps 1 to 3 proved the pieces separately - the captured payloads say the
 * two Mod-Gearman flavours agree byte for byte, and the worker loop answers
 * jobs the test itself put on a queue. What is left is the part only a real
 * core can show: that the NEB module's own job text is the one the module
 * parses, and that the result the module writes is one the core's result
 * thread accepts and files as an active check.
 *
 * So this suite runs the whole chain, twice:
 *
 *   Naemon / Nagios Core  ->  NEB module  ->  gearmand  ->  NSClient++
 *                                          <-  check_results  <-
 *
 * The core containers are the ones step 1 built (`naemon-gearman.Dockerfile`,
 * `nagios-gearman.Dockerfile`), with gearmand inside them and the object
 * config the shared entrypoint writes; NSClient++ runs on the host with
 * `hostgroups = gearman-test`, which is exactly how an agent is deployed.
 *
 * The probe is the core's own status file. Nagios Core has no REST API, and
 * status.dat is the one thing both cores write in the same format - the same
 * mechanism the check_mk site suite uses. It is also the honest probe: it
 * shows what the *core* made of the result, not what the agent thinks it
 * sent, which is the whole point of testing against a real core.
 *
 * Everything that can be tested without Nagios or Naemon is in
 * gearman-worker.test.ts instead; that tier is fast and scripted, so it is
 * where the edge cases live. What is here is what only these images can say.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import {
  GenericContainer,
  NscpInstance,
  Wait,
  adminStatus,
  adminWorkers,
  dockerOrSkip,
  parseStatusDat,
  trackContainerLogs,
  type StartedTestContainer,
  type StatusDat,
} from "@fixtures/index";

jest.setTimeout(900_000);

const FIXTURES = path.resolve(__dirname, "..", "modules", "GearmanClient", "fixtures");
const KEY = fs.readFileSync(path.join(FIXTURES, "key.txt"), "utf8");
const HOSTGROUP = "gearman-test";
const QUEUE = `hostgroup_${HOSTGROUP}`;
/** The host the entrypoint defines, and the name the agent has to answer for. */
const HOSTNAME = "nscp-test";
/** Where the entrypoint puts everything the core writes. */
const STATUS_DAT = "/gearman-test/var/status.dat";
const GEARMAN_PORT = 4730;
/**
 * Fixed, like gearman-worker.test.ts: the restart case stops and starts the
 * container, and a dynamically mapped port is reassigned when it comes back,
 * which would test docker rather than the agent. A different port from that
 * suite's so the two can never collide.
 */
const HOST_PORT = 14732;
const CORES = ["naemon", "nagios"] as const;
type Core = (typeof CORES)[number];

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

dockerOrSkip()("a real Mod-Gearman core driving the agent", () => {
  describe.each(CORES)("%s", (core: Core) => {
    let container: StartedTestContainer;
    let nscp: NscpInstance;
    let scriptsDir: string;
    /** Epoch seconds; every result asserted on has to be newer than this. */
    let startedAt: number;

    /** The core's status file, or an empty one while the core is restarting. */
    async function status(): Promise<StatusDat> {
      try {
        return parseStatusDat((await container.exec(["cat", STATUS_DAT])).output);
      } catch {
        return parseStatusDat("");
      }
    }

    /**
     * Wait for a service the core has (re)checked since `since`, and whose
     * fields satisfy `ok`. Returns the last thing seen either way, so a
     * failure reports what the core actually had rather than `undefined`.
     */
    async function serviceResult(
      description: string,
      ok: (fields: Record<string, string>) => boolean,
      since = startedAt,
      timeoutMs = 180_000,
    ): Promise<Record<string, string>> {
      let last: Record<string, string> = {};
      await waitFor(async () => {
        const found = (await status()).services.get(`${HOSTNAME}!${description}`);
        if (!found) return null;
        last = found;
        return Number(found.last_check) >= since && ok(found) ? found : null;
      }, timeoutMs);
      return last;
    }

    async function hostResult(
      ok: (fields: Record<string, string>) => boolean,
      since = startedAt,
      timeoutMs = 180_000,
    ): Promise<Record<string, string>> {
      let last: Record<string, string> = {};
      await waitFor(async () => {
        const found = (await status()).hosts.get(HOSTNAME);
        if (!found) return null;
        last = found;
        return Number(found.last_check) >= since && ok(found) ? found : null;
      }, timeoutMs);
      return last;
    }

    /**
     * Wait until the agent has announced the hostgroup queue to gearmand with
     * all `expected` of its workers. Returns what gearmand last reported, so
     * "one worker came up and the other did not" fails as itself.
     */
    async function waitForRegistration(expected = 2, timeoutMs = 120_000): Promise<number> {
      let last = 0;
      await waitFor(async () => {
        try {
          const found = (await adminStatus("127.0.0.1", HOST_PORT)).get(QUEUE);
          last = found?.workers ?? 0;
          return last >= expected ? found : null;
        } catch {
          // gearmand goes away with the container during the restart case.
          return null;
        }
      }, timeoutMs);
      return last;
    }

    beforeAll(async () => {
      const image = await GenericContainer.fromDockerfile(
        path.resolve(__dirname),
        `Dockerfiles/${core}-gearman.Dockerfile`,
      ).build(`nscp-it/${core}-gearman`, { deleteOnExit: false });
      container = await trackContainerLogs(
        await image
          .withExposedPorts({ container: GEARMAN_PORT, host: HOST_PORT })
          .withEnvironment({ GEARMAN_KEY: KEY, GEARMAN_HOSTGROUP: HOSTGROUP })
          .withWaitStrategy(Wait.forLogMessage(/Starting \w+ in the foreground/))
          .withStartupTimeout(180_000)
          .start(),
        `${core}-gearman`,
      );

      // The core's `slow` service calls this; the agent has nothing of its
      // own that blocks, and the point is a check that outlives the job's
      // three-second timeout. POSIX only, which is fine: this suite needs
      // docker, and the Windows runners have none.
      scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-gearman-core-"));
      const slow = path.join(scriptsDir, "slow.sh");
      fs.writeFileSync(slow, "#!/bin/sh\nsleep 10\necho 'OK: finally'\n", { mode: 0o755 });

      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": {
          CheckHelpers: "enabled",
          CheckExternalScripts: "enabled",
          // check_cpu, so one of the checks is answered off the running
          // agent's collector - the reason for running the checks inside
          // the agent rather than booting `nscp client -q` per check.
          CheckSystem: "enabled",
          GearmanClient: "enabled",
        },
        "/settings/gearman/worker": {
          server: `127.0.0.1:${HOST_PORT}`,
          key: KEY,
          hostgroups: HOSTGROUP,
          "host names": HOSTNAME,
          workers: "2",
        },
        "/settings/external scripts": { timeout: "60" },
        "/settings/external scripts/scripts": { check_slow: `/bin/sh ${slow}` },
      });
      startedAt = Math.floor(Date.now() / 1000);
      nscp.start();
    });

    afterAll(async () => {
      await nscp?.stop();
      await container?.stop();
      if (scriptsDir) fs.rmSync(scriptsDir, { recursive: true, force: true });
    });

    it("announces the queue the core routes the test hostgroup to", async () => {
      expect(await waitForRegistration()).toBe(2);
      const mine = (await adminWorkers("127.0.0.1", HOST_PORT)).filter((w) =>
        w.functions.includes(QUEUE),
      );
      expect(mine.length).toBe(2);
      // Both workers are this agent, and named so an operator can tell which
      // agent they belong to.
      for (const w of mine) expect(w.clientId).toMatch(/^nscp-.+-\d+$/);
    });

    it("is visible in the core's own gearman tooling", async () => {
      // What an operator on the core box looks at. The flags differ between
      // ConSol's gearman_top and the Nagios fork's, and a non-batch build
      // would sit in its curses loop forever, so the probe is bounded and
      // falls back to gearadmin: the assertion is about the registration
      // being visible from the core side, not about one tool's options.
      const probe = await container.exec([
        "sh",
        "-c",
        `timeout 10 "$GEARMAN_TOP" --host=127.0.0.1:${GEARMAN_PORT} --batch 2>&1 ` +
          `|| gearadmin --port=${GEARMAN_PORT} --status`,
      ]);
      expect(probe.output).toContain(QUEUE);
    });

    it("answers the host check the core scheduled, and the core files it as active", async () => {
      // Wait for the agent's own answer rather than for any result: a check
      // the core scheduled before the agent came up is answered by the NEB
      // module's orphan handling, and that also moves last_check.
      const host = await hostResult((f) => f.plugin_output.includes("host-is-up"));
      expect(host.plugin_output).toContain("host-is-up");
      // 0 is UP; the wrapped check_ok answered OK and check_always_ok kept it.
      expect(host.current_state).toBe("0");
      // 0 is an active check: the core believes it ran the plugin itself,
      // which is exactly what Mod-Gearman is for.
      expect(host.check_type).toBe("0");
      expect(Number(host.last_check)).toBeGreaterThanOrEqual(startedAt);
    });

    it("answers a service check with the query's own message", async () => {
      const helper = await serviceResult("helper", (f) => f.plugin_output.includes("hello"));
      expect(helper.plugin_output).toContain("hello");
      expect(helper.current_state).toBe("0");
      expect(helper.check_type).toBe("0");
    });

    it("hands the core performance data from a collector-backed check", async () => {
      // check_cpu reads the agent's own 1 Hz collector. The thresholds can
      // never hold, so the state is deterministic wherever this runs; what
      // matters is that the perf data survives the `message|perfdata` line
      // the core parses.
      const cpu = await serviceResult("cpu", (f) => f.performance_data.includes("total 5m"));
      expect(cpu.current_state).toBe("0");
      expect(cpu.performance_data).toContain("total 5m");
      expect(cpu.plugin_output).not.toContain("|");
    });

    it("reports the configured timeout return when a check overruns the job", async () => {
      // The core allows three seconds (service_check_timeout); the script
      // sleeps ten. The agent answers on the job's own deadline rather than
      // leaving the core to its orphan timeout.
      //
      // That the core shows the agent's own wording is the point of
      // `exited_ok=1` in the result: a 0 there makes it discard the output and
      // report that the plugin did not exit properly instead.
      const slow = await serviceResult("slow", (f) => f.plugin_output === "(Check Timed Out)");
      expect(slow.plugin_output).toBe("(Check Timed Out)");
      // `timeout return` defaults to 2, CRITICAL.
      expect(slow.current_state).toBe("2");
      expect(slow.check_type).toBe("0");
    });

    it("picks the checks back up after the core and its job server restart", async () => {
      await container.restart();
      // The registration is server state, and gearmand lost it with the
      // container, so seeing it again means the agent noticed and announced
      // itself - not merely that the port is open again.
      expect(await waitForRegistration(2, 180_000)).toBe(2);
      const after = Math.floor(Date.now() / 1000);
      const helper = await serviceResult("helper", (f) => f.plugin_output.includes("hello"), after);
      expect(helper.plugin_output).toContain("hello");
      expect(Number(helper.last_check)).toBeGreaterThanOrEqual(after);
    });
  });
});
