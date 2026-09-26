/**
 * GearmanClient plan, steps 3 and 5: the worker loop, in both modes, against
 * a real gearmand.
 *
 * The test plays the monitoring core. It puts encrypted check jobs on the
 * queue the NEB module would submit to, then registers on the result queue
 * and reads back what the agent answered - the two halves of the core's job
 * that the worker can actually see. Nothing here needs Nagios or Naemon; the
 * real cores come in step 4, and what they add is only that their NEB modules
 * agree with the fixture about the bytes, which step 1 already showed.
 *
 * Everything else is deliberately here rather than there: this tier is fast
 * and deterministic, so it is where the edge cases live.
 */
import execa from "execa";
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import {
  GearmanWorker,
  GenericContainer,
  NscpInstance,
  Wait,
  adminStatus,
  adminWorkers,
  externalGearmand,
  formatCoreTime,
  gearmandOrSkip,
  grabPayload,
  runQueue,
  submitCheckJob,
  trackContainerLogs,
  type CheckJob,
  type EnvelopeOptions,
  type GearmanServer,
  type StartedTestContainer,
  onWindows,
} from "@fixtures/index";

jest.setTimeout(600_000);

const KEY = "nscp-test-key";
/**
 * Fixed, so the reconnect case can restart the container and still find it:
 * a dynamically mapped port is reassigned when a stopped container starts
 * again, which would test docker rather than the agent.
 */
const HOST_PORT = 14731;
const HOSTNAME = "nscp-test";

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

async function waitFor<T>(produce: () => Promise<T | null>, timeoutMs = 60_000): Promise<T | null> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const value = await produce();
    if (value !== null) return value;
    if (Date.now() >= deadline) return null;
    await sleep(250);
  }
}

gearmandOrSkip()("Mod-Gearman worker", () => {
  let gearmand: StartedTestContainer | undefined;
  let server: GearmanServer;
  let scriptsDir: string;

  /**
   * A script the agent can run as an external check. POSIX only: the cases
   * that use one guard on `onWindows`, and the suite needs docker anyway,
   * which the Windows runners do not have.
   */
  function writeScript(name: string, body: string): string {
    const file = path.join(scriptsDir, `${name}.sh`);
    fs.writeFileSync(file, body, { mode: 0o755 });
    return `/bin/sh ${file}`;
  }

  beforeAll(async () => {
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-gearman-"));
    const external = externalGearmand();
    if (external) {
      server = external;
      return;
    }
    const image = await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/gearmand.Dockerfile",
    ).build("nscp-it/gearmand", { deleteOnExit: false });
    gearmand = await trackContainerLogs(
      await image
        .withExposedPorts({ container: 4730, host: HOST_PORT })
        .withWaitStrategy(Wait.forListeningPorts())
        .start(),
      "gearmand",
    );
    server = { host: "127.0.0.1", port: HOST_PORT };
  });

  afterAll(async () => {
    await gearmand?.stop();
  });

  // -------------------------------------------------------------------------
  // Harness
  // -------------------------------------------------------------------------

  interface Agent {
    nscp: NscpInstance;
    /** The queue the core would submit this agent's checks to. */
    queue: string;
    /** The queue the agent answers on, which the reader below is registered for. */
    resultQueue: string;
    /** Read the next result the agent submits, or null if none arrives in time. */
    nextResult(envelope?: EnvelopeOptions): Promise<Record<string, string> | null>;
    /** Put a job on this agent's queue, as the NEB module would. */
    submit(job: Partial<CheckJob>, envelope?: EnvelopeOptions): Promise<void>;
    /** Re-register the result reader after gearmand has been restarted. */
    reopenReader(): Promise<void>;
    stop(): Promise<void>;
  }

  /**
   * One agent with its own hostgroup and its own result queue, so the blocks
   * sharing this gearmand never see each other's jobs or answers.
   */
  async function startAgent(
    group: string,
    worker: Record<string, string> = {},
    extra: Record<string, Record<string, string>> = {},
  ): Promise<Agent> {
    // The suffix goes on the group, not on the queue: the agent builds its
    // own queue name from `hostgroups`, so the two have to agree.
    const runGroup = runQueue(group);
    const queue = `hostgroup_${runGroup}`;
    const resultQueue = `results_${runGroup}`;
    const nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled",
        CheckExternalScripts: "enabled",
        GearmanClient: "enabled",
      },
      "/settings/gearman/worker": {
        server: `${server.host}:${server.port}`,
        key: KEY,
        hostgroups: runGroup,
        "host names": HOSTNAME,
        workers: "1",
        ...worker,
      },
      ...extra,
    });
    nscp.start();

    let reader = await GearmanWorker.connect(server.host, server.port, [resultQueue]);

    return {
      nscp,
      queue,
      resultQueue,
      async nextResult(envelope: EnvelopeOptions = { key: KEY }) {
        const grabbed = await grabPayload(reader, envelope, 60_000);
        if (!grabbed) return null;
        await reader.complete(grabbed.job);
        return grabbed.fields as Record<string, string>;
      },
      async submit(job: Partial<CheckJob>, envelope: EnvelopeOptions = { key: KEY }) {
        await submitCheckJob(
          server,
          queue,
          {
            type: "service",
            host_name: HOSTNAME,
            service_description: "helper",
            command_line: "check_ok",
            result_queue: resultQueue,
            core_time: formatCoreTime(),
            timeout: 30,
            ...job,
          } as CheckJob,
          envelope,
        );
      },
      async reopenReader() {
        reader.close();
        reader = await GearmanWorker.connect(server.host, server.port, [resultQueue]);
      },
      async stop() {
        reader.close();
        await nscp.stop();
      },
    };
  }

  /** Wait until the agent has announced `queue` to gearmand. */
  async function waitForRegistration(queue: string): Promise<boolean> {
    const found = await waitFor(async () => {
      try {
        const entry = (await adminStatus(server.host, server.port)).get(queue);
        return entry && entry.workers > 0 ? entry : null;
      } catch {
        // The server may still be coming back up after a restart.
        return null;
      }
    });
    return found !== null;
  }

  /** Wait until `queue` holds at least `count` jobs, as gearmand counts them. */
  async function waitForQueued(queue: string, count: number): Promise<boolean> {
    const found = await waitFor(async () => {
      try {
        const entry = (await adminStatus(server.host, server.port)).get(queue);
        return entry && entry.queued >= count ? entry : null;
      } catch {
        return null;
      }
    }, 30_000);
    return found !== null;
  }

  // -------------------------------------------------------------------------
  // Registration and the happy path
  // -------------------------------------------------------------------------

  describe("a worker on its hostgroup queue", () => {
    let agent: Agent;

    beforeAll(async () => {
      agent = await startAgent(
        "worker-basic",
        {},
        {
          "/settings/external scripts": { timeout: "60" },
          "/settings/external scripts/scripts": {
            perf: writeScript("perf", "#!/bin/sh\necho \"OK: disk is fine|'used'=42%;80;90\"\n"),
          },
        },
      );
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("shows up in the admin protocol under a name that identifies it", async () => {
      const mine = (await adminWorkers(server.host, server.port)).filter((w) =>
        w.functions.includes(agent.queue),
      );
      expect(mine.length).toBe(1);
      // What an operator reads in gearman_top; the trailing number is the
      // worker within this agent.
      expect(mine[0].clientId).toMatch(/^nscp-.+-1$/);
    });

    it("runs a service check and answers on the job's result queue", async () => {
      await agent.submit({ command_line: "check_ok message=hello" });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result).toMatchObject({
        type: "active",
        host_name: HOSTNAME,
        service_description: "helper",
        return_code: "0",
        exited_ok: "1",
      });
      expect(result!.output).toContain("hello");
      // So an operator reading the core's status can tell which agent answered.
      expect(result!.source).toMatch(/^NSClient\+\+ .+ on /);
      // Quoted back from the job so the core can work out its own latency.
      expect(result!.core_start_time).toMatch(/^\d{10}\.\d{6}$/);
      expect(result!.start_time).toMatch(/^\d{10}\.\d{6}$/);
      expect(Number(result!.finish_time)).toBeGreaterThanOrEqual(Number(result!.start_time));
    });

    it("answers a host check with no service_description", async () => {
      await agent.submit({
        type: "host",
        service_description: undefined,
        command_line: "check_ok message=up",
      });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.host_name).toBe(HOSTNAME);
      expect(result!.service_description).toBeUndefined();
      expect(result!.return_code).toBe("0");
    });

    it("files a second result for the same service beside the first", async () => {
      // gearmand coalesces a background job onto one already queued under the
      // same function and unique id: the older handle comes back and the new
      // payload is dropped. A result therefore carries no unique id at all,
      // because the case it would break is precisely the one that matters -
      // the core is behind, so the previous result for this service is still
      // sitting on the queue when the next one arrives. Nothing reads the
      // result queue until both are on it, which is what holds that window
      // open here.
      //
      // The *job* queue coalesces the same way, and there it is wanted - it is
      // what mod_gearman's use_uniq_jobs is for, and the NEB module does send
      // a unique id on a job. So the second check is queued only once the
      // first has come back as a result: two identical checks queued at once
      // would collapse before the agent ever saw the second, which is a
      // different mechanism from the one under test.
      await agent.submit({ command_line: "check_ok message=first" });
      expect(await waitForQueued(agent.resultQueue, 1)).toBe(true);
      await agent.submit({ command_line: "check_ok message=second" });
      expect(await waitForQueued(agent.resultQueue, 2)).toBe(true);

      const first = await agent.nextResult();
      const second = await agent.nextResult();
      expect(first).not.toBeNull();
      expect(second).not.toBeNull();
      const outputs = [first!.output, second!.output].sort();
      expect(outputs[0]).toContain("first");
      expect(outputs[1]).toContain("second");
    });

    it("carries the check's status through", async () => {
      await agent.submit({ command_line: "check_critical message=bad" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("2");
      expect(result!.output).toContain("bad");
    });

    it("puts performance data on the same line the core parses", async () => {
      if (onWindows) return;
      await agent.submit({ command_line: "perf" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      // `message|perfdata`, exactly as the core would read a plugin's stdout.
      expect(result!.output).toBe("OK: disk is fine|'used'=42%;80;90");
    });

    it("refuses a job for a host it does not answer for, and says so", async () => {
      await agent.submit({ host_name: "some-other-host", command_line: "check_ok message=nope" });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.host_name).toBe("some-other-host");
      expect(result!.return_code).toBe("3");
      expect(result!.output).toContain("does not answer for some-other-host");
      // The check must not have run: that is the whole point of the binding.
      expect(result!.output).not.toContain("nope");
    });

    it("keeps working after a refused job", async () => {
      await agent.submit({ command_line: "check_ok message=recovered" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("recovered");
    });
  });

  // -------------------------------------------------------------------------
  // Timeouts
  // -------------------------------------------------------------------------

  describe("a check that overruns the job's timeout", () => {
    let agent: Agent;

    beforeAll(async () => {
      // A check that genuinely does not return in time: CheckHelpers has
      // nothing that sleeps.
      agent = await startAgent(
        "worker-timeout",
        { "timeout return": "2" },
        {
          "/settings/external scripts": { timeout: "60" },
          "/settings/external scripts/scripts": {
            slow: writeScript("slow", "#!/bin/sh\nsleep 20\n"),
          },
        },
      );
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("reports the configured status and the core's own wording", async () => {
      if (onWindows) return;
      await agent.submit({ service_description: "slow", command_line: "slow", timeout: 2 });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.return_code).toBe("2");
      expect(result!.output).toBe("(Check Timed Out)");
      // Not 0: exited_ok=0 makes the core throw the output away and report
      // "did not exit properly" instead of the reason we just worked out.
      expect(result!.exited_ok).toBe("1");
    });

    it("goes on to the next job rather than waiting for the overrun to end", async () => {
      if (onWindows) return;
      await agent.submit({ command_line: "check_ok message=after" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("after");
    });
  });

  // -------------------------------------------------------------------------
  // Proxy mode
  // -------------------------------------------------------------------------

  describe("a worker in proxy mode", () => {
    let agent: Agent;

    beforeAll(async () => {
      agent = await startAgent(
        "worker-proxy",
        // `host names` is set as well, and deliberately: a proxy is normally
        // configured by taking an agent's configuration and changing the one
        // line, and the leftover must not narrow what it answers for.
        { mode: "proxy", "host names": HOSTNAME },
        {
          // The target the core expanded is an argument, and an external
          // script takes none unless it is told to.
          "/settings/external scripts": { timeout: "60", "allow arguments": "true" },
          "/settings/external scripts/scripts": {
            // Stands in for check_nrpe and friends: what matters here is that
            // the target the core expanded into the command line arrives at
            // the check. The real remote chain is gearman-proxy.test.ts.
            // `$ARG1$` because an external script substitutes its arguments
            // rather than appending them.
            target: `${writeScript("target", '#!/bin/sh\necho "OK: asked $1"\n')} $ARG1$`,
          },
        },
      );
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("runs a service check for a host it is not", async () => {
      await agent.submit({
        host_name: "win-db01",
        service_description: "CPU load",
        command_line: "check_ok message=answered-for-win-db01",
      });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      // Answered under the core's own host name, not the proxy's: the core
      // files the result against the host it scheduled the check for.
      expect(result!.host_name).toBe("win-db01");
      expect(result!.service_description).toBe("CPU load");
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("answered-for-win-db01");
    });

    it("runs a host check for a host it is not", async () => {
      await agent.submit({
        type: "host",
        host_name: "win-srv02",
        service_description: undefined,
        command_line: "check_ok message=win-srv02-is-up",
      });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.type).toBe("active");
      expect(result!.host_name).toBe("win-srv02");
      expect(result!.service_description).toBeUndefined();
      expect(result!.output).toContain("win-srv02-is-up");
    });

    it("hands the check the target the core expanded into the command line", async () => {
      if (onWindows) return;
      // What `nscp!check_nrpe -H $HOSTADDRESS$ …` amounts to: the job carries
      // the target, and the check has to receive it as an argument of its own.
      await agent.submit({ host_name: "win-db01", command_line: "target 10.0.0.5" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toBe("OK: asked 10.0.0.5");
    });

    it("says on start what it will run, and that the host names no longer apply", () => {
      expect(agent.nscp.capturedStdout()).toContain("running in proxy mode");
      expect(agent.nscp.capturedStdout()).toContain(`hostgroup_worker-proxy`);
      expect(agent.nscp.capturedStdout()).toContain("no effect in proxy mode");
    });
  });

  // -------------------------------------------------------------------------
  // max age
  // -------------------------------------------------------------------------

  describe("max age", () => {
    let agent: Agent;

    beforeAll(async () => {
      agent = await startAgent("worker-maxage", { "max age": "30" });
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("discards a job queued before an outage instead of running it late", async () => {
      await agent.submit({
        command_line: "check_ok message=stale",
        core_time: formatCoreTime(new Date(Date.now() - 600_000)),
      });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.return_code).toBe("3");
      expect(result!.output).toContain("max age");
      expect(result!.output).not.toContain("stale");
    });

    it("leaves a job that is still fresh alone", async () => {
      await agent.submit({ command_line: "check_ok message=fresh" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("fresh");
    });
  });

  // -------------------------------------------------------------------------
  // Encryption
  // -------------------------------------------------------------------------

  describe("a job encrypted with the wrong key", () => {
    let agent: Agent;

    beforeAll(async () => {
      agent = await startAgent("worker-wrongkey");
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("is refused rather than guessed at, and never logged", async () => {
      await agent.submit(
        { command_line: "check_ok message=shouldnotrun" },
        { key: "a-completely-different-key" },
      );
      const logged = await waitFor(
        async () => (agent.nscp.capturedStdout().includes("wrong key?") ? true : null),
        30_000,
      );
      expect(logged).toBe(true);
      // Neither the ciphertext nor whatever the wrong key turned it into.
      expect(agent.nscp.capturedStdout()).not.toContain("shouldnotrun");
    });

    it("goes on to the next job", async () => {
      await agent.submit({ command_line: "check_ok message=recovered" });
      const result = await agent.nextResult();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("recovered");
    });
  });

  describe("encryption disabled", () => {
    let agent: Agent;
    const plain: EnvelopeOptions = { encryption: false, key: "" };

    beforeAll(async () => {
      agent = await startAgent("worker-plain", { encryption: "false", insecure: "true", key: "" });
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("runs a base64-only job and answers in the same form", async () => {
      await agent.submit({ command_line: "check_ok message=plaintext" }, plain);
      const result = await agent.nextResult(plain);
      expect(result).not.toBeNull();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("plaintext");
    });

    it("says on every start that the payloads are not protected", () => {
      expect(agent.nscp.capturedStdout()).toContain("encryption disabled");
    });
  });

  describe("a misconfigured worker", () => {
    /** Boot an agent that loads the module, then exit, and return what it said. */
    async function bootWith(worker: Record<string, string>): Promise<string> {
      const nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { GearmanClient: "enabled" },
        "/settings/gearman/worker": worker,
      });
      // `nscp test` reads commands from stdin, so an immediate `exit` boots
      // every module and comes straight back with the log on stdout.
      const result = await nscp.run(["test"], {
        input: "exit\n",
        timeout: 60_000,
        allowFailure: true,
      });
      return result.all ?? `${result.stdout}\n${result.stderr}`;
    }

    it("refuses to start encrypted with no key rather than using a well-known one", async () => {
      expect(
        await bootWith({ server: `${server.host}:${server.port}`, hostgroups: "nokey" }),
      ).toContain("no key is set");
    });

    it("refuses to start unencrypted unless that is said explicitly", async () => {
      const output = await bootWith({
        server: `${server.host}:${server.port}`,
        hostgroups: "noinsecure",
        encryption: "false",
      });
      expect(output).toContain("'insecure' is not set");
    });

    it("refuses to start with no queue to answer", async () => {
      expect(await bootWith({ server: `${server.host}:${server.port}`, key: KEY })).toContain(
        "no queue to answer",
      );
    });

    it("refuses to start in a mode it does not know rather than picking one", async () => {
      // Silently falling back to agent would leave a proxy refusing every
      // check it was deployed for; silently falling back to proxy would run
      // every host's checks on a host that was meant to answer for itself.
      const output = await bootWith({
        server: `${server.host}:${server.port}`,
        key: KEY,
        hostgroups: "badmode",
        mode: "gateway",
      });
      expect(output).toContain("unknown mode 'gateway'");
    });

    it("refuses to start with no server to connect to", async () => {
      expect(await bootWith({ key: KEY, hostgroups: "noserver" })).toContain(
        "no job server configured",
      );
    });
  });

  // -------------------------------------------------------------------------
  // Settings reload
  // -------------------------------------------------------------------------

  describe("a settings reload while jobs are flowing", () => {
    // The thing a reload breaks if the module does not stop its own threads
    // first: loadModuleEx runs again on the live module, and the previous
    // workers stay registered on the queue. Every reload then doubles them,
    // and a check is answered twice.
    //
    // The console is driven over a pipe rather than through NscpInstance
    // because `reload` is typed at the `nscp test` prompt.
    it("leaves exactly one worker registered, and it still answers", async () => {
      const group = runQueue("worker-reload");
      const queue = `hostgroup_${group}`;
      const resultQueue = `results_${group}`;
      const nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { CheckHelpers: "enabled", GearmanClient: "enabled" },
        "/settings/gearman/worker": {
          server: `${server.host}:${server.port}`,
          key: KEY,
          hostgroups: group,
          "host names": HOSTNAME,
          workers: "2",
        },
      });

      const overrides: string[] = [];
      for (const [k, v] of Object.entries(nscp.pathOverrides))
        overrides.push("--path-override", `${k}=${v}`);
      const proc = execa(
        process.env.NSCP_BIN as string,
        ["test", "--settings", nscp.settingsFile, ...overrides],
        { cwd: nscp.workDir, all: true, timeout: 180_000, reject: false, env: process.env },
      );
      const stdin = proc.stdin;
      if (!stdin) throw new Error("no stdin pipe");
      const reader = await GearmanWorker.connect(server.host, server.port, [resultQueue]);

      async function runOne(message: string): Promise<Record<string, string> | null> {
        await submitCheckJob(
          server,
          queue,
          {
            type: "service",
            host_name: HOSTNAME,
            service_description: "helper",
            command_line: `check_ok message=${message}`,
            result_queue: resultQueue,
            core_time: formatCoreTime(),
            timeout: 30,
          } as CheckJob,
          { key: KEY },
        );
        const grabbed = await grabPayload(reader, { key: KEY }, 60_000);
        if (!grabbed) return null;
        await reader.complete(grabbed.job);
        return grabbed.fields as Record<string, string>;
      }

      try {
        expect(await waitForRegistration(queue)).toBe(true);
        expect((await adminStatus(server.host, server.port)).get(queue)?.workers).toBe(2);
        expect((await runOne("before"))?.output).toContain("before");

        stdin.write("reload\n");
        // The reload is queued and runs on a core thread a moment later.
        await sleep(10_000);

        // Two, not four: the module stopped its own workers before starting
        // the new ones.
        const after = await waitFor(async () => {
          const entry = (await adminStatus(server.host, server.port)).get(queue);
          return entry && entry.workers === 2 ? entry : null;
        }, 30_000);
        expect(after?.workers).toBe(2);

        const result = await runOne("after");
        expect(result?.output).toContain("after");
        // And only one worker answered: a duplicate registration answers the
        // same check twice, which is what the core sees as a flapping service.
        expect(await grabPayload(reader, { key: KEY }, 3_000)).toBeNull();
      } finally {
        reader.close();
        stdin.write("exit\n");
        stdin.end();
        await proc;
      }
    });
  });

  // -------------------------------------------------------------------------
  // Reconnect
  // -------------------------------------------------------------------------

  /**
   * Restarting the job server is the one thing only the container can do, so
   * this block skips when the suite was pointed at an external gearmand.
   */
  const reconnectDescribe = externalGearmand() ? describe.skip : describe;
  reconnectDescribe("when gearmand goes away", () => {
    let agent: Agent;

    beforeAll(async () => {
      agent = await startAgent("worker-reconnect");
      expect(await waitForRegistration(agent.queue)).toBe(true);
    });

    afterAll(async () => {
      await agent?.stop();
    });

    it("reconnects, re-registers and answers again", async () => {
      await gearmand!.restart();
      // The registration is server state, so seeing it again means the agent
      // noticed and announced itself - not merely that the port is open.
      expect(await waitForRegistration(agent.queue)).toBe(true);
      await agent.reopenReader();

      await agent.submit({ command_line: "check_ok message=backagain" });
      const result = await agent.nextResult();
      expect(result).not.toBeNull();
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("backagain");
    });
  });
});
