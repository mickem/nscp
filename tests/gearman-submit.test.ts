/**
 * GearmanClient plan, step 6: the passive submit channel.
 *
 * Mod-Gearman's result queue is not only for workers. The core's result thread
 * files whatever arrives on `check_results`, and a result that says
 * `type=passive` is filed as a passive check - which is all `send_gearman`
 * does. So a Mod-Gearman installation can drop NSCA and let the agent push its
 * scheduled results into the same gearmand the checks come from.
 *
 * The test plays the core's result thread: it registers on the result queue,
 * reads what the agent submitted, decrypts it and asserts on the text. Every
 * way in is covered, because they parse their arguments differently and have
 * broken independently before:
 *
 *   - the one-shot client path, `nscp client --boot --query submit_gearman`
 *   - the same command over REST, where a boolean arrives as the single token
 *     `encryption=true` (the `bool_switch` trap in CLAUDE.md)
 *   - a Scheduler entry with `channel = GEARMAN`, which is the deployment
 *
 * The worker half has its own suite (gearman-worker.test.ts); what little of
 * it appears here is the metrics, which only mean anything with a pool running.
 */
import * as path from "path";
import request from "supertest";
import {
  GearmanWorker,
  GenericContainer,
  NscpInstance,
  REST_URL,
  Wait,
  externalGearmand,
  gearmandOrSkip,
  grabPayload,
  runQueue,
  trackContainerLogs,
  type EnvelopeOptions,
  type GearmanServer,
  type StartedTestContainer,
} from "@fixtures/index";

jest.setTimeout(600_000);

const KEY = "nscp-test-key";
/** Its own port: the worker and core suites hold 14731 and 14732. */
const HOST_PORT = 14733;

function sleep(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

gearmandOrSkip()("Mod-Gearman submit channel", () => {
  let gearmand: StartedTestContainer | undefined;
  let server: GearmanServer;

  beforeAll(async () => {
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

  /**
   * Read one result off `queue` and hand back its fields, or null if none
   * arrives. A fresh registration per call, so a result left behind by an
   * earlier case cannot be mistaken for this one's: gearmand holds a
   * background job until somebody registers for its queue, and every block
   * below therefore submits to a queue of its own.
   */
  async function nextResult(
    queue: string,
    envelope: EnvelopeOptions = { key: KEY },
    timeoutMs = 60_000,
  ): Promise<Record<string, string> | null> {
    const reader = await GearmanWorker.connect(server.host, server.port, [queue]);
    try {
      const grabbed = await grabPayload(reader, envelope, timeoutMs);
      if (!grabbed) return null;
      await reader.complete(grabbed.job);
      return grabbed.fields as Record<string, string>;
    } finally {
      reader.close();
    }
  }

  // ---------------------------------------------------------------------
  // The one-shot client path
  // ---------------------------------------------------------------------

  describe("submit_gearman from the command line", () => {
    let nscp: NscpInstance;

    /** Run submit_gearman against a configured instance; returns its output. */
    async function submit(args: string[]): Promise<string> {
      const r = await nscp.run(
        ["client", "--module", "GearmanClient", "--boot", "--query", "submit_gearman", ...args],
        { allowFailure: true },
      );
      return r.all ?? `${r.stdout}\n${r.stderr}`;
    }

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": { GearmanClient: "enabled" },
        // A configured target, so the block covers both ways of naming the
        // gearmand: this one, and the bare address= below.
        "/settings/gearman/client/targets/default": {
          address: `${server.host}:${server.port}`,
          key: KEY,
          queue: runQueue("submit_default"),
        },
      });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("files a service result the core's result thread would accept", async () => {
      const reading = nextResult(runQueue("submit_default"));
      const out = await submit(["command=cpu", "result=1", "message=cpu is busy|'load'=80%;70;90"]);
      expect(out).toContain("Submission successful");

      const result = await reading;
      expect(result).not.toBeNull();
      expect(result).toMatchObject({
        // What makes the core file this as a passive check rather than
        // believe it scheduled the check itself.
        type: "passive",
        service_description: "cpu",
        return_code: "1",
        // Without this the core throws the output away and reports that the
        // plugin did not exit properly instead.
        exited_ok: "1",
      });
      // Message and performance data on the one line the core parses.
      expect(result!.output).toBe("cpu is busy|'load'=80%;70;90");
      // So an operator reading the core can tell which agent filed it.
      expect(result!.source).toMatch(/^NSClient\+\+ .+ on /);
      expect(result!.start_time).toMatch(/^\d{10}\.\d{6}$/);
      expect(result!.host_name.length).toBeGreaterThan(0);
    });

    it("files a host result with no service_description", async () => {
      // The `host_check` alias is the convention NSCA and NRDP use, and a
      // result with no service_description is a host result to both cores.
      const reading = nextResult(runQueue("submit_default"));
      await submit(["alias=host_check", "result=2", "message=host is down"]);

      const result = await reading;
      expect(result).not.toBeNull();
      expect(result!.type).toBe("passive");
      expect(result!.service_description).toBeUndefined();
      expect(result!.return_code).toBe("2");
      expect(result!.output).toBe("host is down");
    });

    it("honours a gearmand named on the command line, and its own queue", async () => {
      const reading = nextResult(runQueue("submit_adhoc"));
      const out = await submit([
        `address=${server.host}:${server.port}`,
        `key=${KEY}`,
        `queue=${runQueue("submit_adhoc")}`,
        "command=adhoc",
        "result=0",
        "message=one off",
      ]);
      expect(out).toContain("Submission successful");
      expect((await reading)!.output).toBe("one off");
    });

    it("sends unencrypted only when that is said out loud", async () => {
      // encryption=false alone is refused: the payload would be plain base64,
      // readable and forgeable by anyone who can reach gearmand.
      const refused = await submit([
        `address=${server.host}:${server.port}`,
        "encryption=false",
        "command=plain",
        "result=0",
        "message=nope",
      ]);
      expect(refused).toMatch(/insecure/i);
      expect(refused).not.toContain("Submission successful");

      // With `insecure` it goes, and the payload really is plain base64 - the
      // reader below is given no key at all.
      const reading = nextResult(runQueue("submit_plain"), { key: "", encryption: false });
      const sent = await submit([
        `address=${server.host}:${server.port}`,
        "encryption=false",
        "insecure=true",
        `queue=${runQueue("submit_plain")}`,
        "command=plain",
        "result=0",
        "message=in the clear",
      ]);
      expect(sent).toContain("Submission successful");
      expect((await reading)!.output).toBe("in the clear");
    });

    it("reports a gearmand that is not there rather than claiming success", async () => {
      // A background job is fire and forget on the wire, so without waiting
      // for the acknowledgement a submission would report success for a result
      // that never left the socket - the one thing a passive channel must not
      // do, since nobody is waiting for the check on the other side either.
      const out = await submit([
        "address=127.0.0.1:1",
        `key=${KEY}`,
        "command=unreachable",
        "result=0",
        "message=nope",
      ]);
      expect(out).not.toContain("Submission successful");
    });
  });

  // ---------------------------------------------------------------------
  // The refusals, with nothing to fall back on
  // ---------------------------------------------------------------------

  describe("submit_gearman with no target configured", () => {
    // These cannot live in the block above, and the reason is the point of
    // this one. `client::configuration::get_target` applies the `default`
    // target object whenever the named one is not found, and applies it
    // *under* whatever the command line says - so with a `default` in the
    // settings, `address=` alone still inherits that target's key, and
    // `target=missing` quietly becomes `target=default`. Both refusals are
    // real (verified below) but neither is reachable from an agent that has
    // a working target configured, which is why they have an instance with
    // no `/settings/gearman/client/targets` at all.
    let nscp: NscpInstance;

    async function submit(args: string[]): Promise<string> {
      const r = await nscp.run(
        ["client", "--module", "GearmanClient", "--boot", "--query", "submit_gearman", ...args],
        { allowFailure: true },
      );
      return r.all ?? `${r.stdout}\n${r.stderr}`;
    }

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({ "/modules": { GearmanClient: "enabled" } });
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("refuses an encrypted submission with no key", async () => {
      // The key is the only thing separating a result this agent filed from
      // one anybody who can reach gearmand made up.
      const out = await submit([
        `address=${server.host}:${server.port}`,
        "command=nokey",
        "result=0",
        "message=nope",
      ]);
      expect(out).toMatch(/no key/i);
      expect(out).not.toContain("Submission successful");
    });

    it("says where to name a gearmand when none was", async () => {
      const out = await submit(["command=nowhere", "result=0", "message=nope"]);
      expect(out).toMatch(/address=<host>:<port>|targets/i);
      expect(out).not.toContain("Submission successful");
    });

    it("says the same for a target name that does not exist", async () => {
      // Nothing to fall back to here, so the unknown name reads as what it
      // is. With a `default` configured it would have been submitted there
      // instead - the framework's behaviour, not this module's.
      const out = await submit(["command=nowhere", "result=0", "message=nope", "target=missing"]);
      expect(out).toMatch(/address=<host>:<port>|targets/i);
      expect(out).not.toContain("Submission successful");
    });
  });

  // ---------------------------------------------------------------------
  // The channel, REST, and the metrics
  // ---------------------------------------------------------------------

  describe("the channel a Scheduler submits to", () => {
    let nscp: NscpInstance;
    let key: string;

    beforeAll(async () => {
      nscp = new NscpInstance();
      await nscp.configure({
        "/modules": {
          GearmanClient: "enabled",
          CheckHelpers: "enabled",
          Scheduler: "enabled",
          WEBServer: "enabled",
        },
        "/settings/default": {
          password: "default-password",
          "allowed hosts": "127.0.0.1,::1",
        },
        "/settings/WEB/server/roles": { full: "*" },
        "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
        // Push metrics every second so the assertions below do not wait out
        // the ten second default.
        "/settings/core": { "metrics interval": "1s" },
        "/settings/gearman/client": { channel: "GEARMAN" },
        "/settings/gearman/client/targets/default": {
          address: `${server.host}:${server.port}`,
          key: KEY,
          queue: runQueue("submit_scheduled"),
        },
        // A worker as well, so the metrics below describe a real pool. Its own
        // hostgroup, and nothing ever queues a job there: what is asserted is
        // that the counters exist and that the connection is reported.
        "/settings/gearman/worker": {
          server: `${server.host}:${server.port}`,
          key: KEY,
          hostgroups: "submit-metrics",
          workers: "1",
        },
        "/settings/scheduler/schedules/default": {
          channel: "GEARMAN",
          interval: "2s",
          // check_ok returns OK, which is filtered out otherwise.
          report: "all",
        },
        "/settings/scheduler/schedules/sched_check": {
          command: 'check_ok "message=from the scheduler"',
        },
      });

      await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
      nscp.start();
      await nscp.waitForPort(8443, { timeoutMs: 30_000 });
      const login = await request(REST_URL)
        .get("/api/v2/login")
        .auth("admin", "default-password")
        .trustLocalhost(true)
        .expect(200);
      key = login.body.key as string;
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("pushes a scheduled check into the result queue with no NSCA involved", async () => {
      const result = await nextResult(runQueue("submit_scheduled"));
      expect(result).not.toBeNull();
      expect(result!.type).toBe("passive");
      // The schedule's name is the service the core files it under.
      expect(result!.service_description).toBe("sched_check");
      expect(result!.return_code).toBe("0");
      expect(result!.output).toContain("from the scheduler");
    });

    it("accepts a valued boolean over REST, where the flag is one token", async () => {
      // `encryption=true` arrives as a single token over REST. A boolean
      // declared as a bool_switch rejects exactly that with "does not take any
      // arguments", and only over REST - which is how it ships broken.
      const reading = nextResult(runQueue("submit_rest"));
      const res = await request(REST_URL)
        .get("/api/v1/queries/submit_gearman/commands/execute")
        .query({
          address: `${server.host}:${server.port}`,
          key: KEY,
          queue: runQueue("submit_rest"),
          encryption: "true",
          command: "rest_check",
          result: "1",
          message: "from rest",
        })
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect(res.body.result).toBe(0);

      const result = await reading;
      expect(result).not.toBeNull();
      expect(result!.service_description).toBe("rest_check");
      expect(result!.return_code).toBe("1");
      expect(result!.output).toBe("from rest");
    });

    it("publishes what the worker pool has done", async () => {
      // Give the metrics task a tick to run after the workers connected.
      await sleep(3_000);
      const res = await request(REST_URL)
        .get("/api/v2/openmetrics")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      const text = res.text as string;

      // Jobs and errors only ever grow, so a scraper may rate() them; the
      // other two describe the pool right now.
      expect(text).toContain("# TYPE gearman_worker_jobs_total counter");
      expect(text).toContain("# TYPE gearman_worker_errors_total counter");
      expect(text).toContain("# TYPE gearman_worker_connected gauge");
      // The unit is part of the family name in the exposition.
      expect(text).toContain("# UNIT gearman_worker_last_job_age_seconds seconds");
      // Every one of them declares what it means.
      for (const family of [
        "gearman_worker_jobs_total",
        "gearman_worker_errors_total",
        "gearman_worker_connected",
        "gearman_worker_last_job_age_seconds",
      ]) {
        expect(text).toMatch(new RegExp(`^# HELP ${family} \\S`, "m"));
      }

      const connected = /^gearman_worker_connected (-?\d+)$/m.exec(text);
      expect(connected).not.toBeNull();
      // The one configured worker is attached to gearmand.
      expect(Number(connected![1])).toBe(1);

      // -1, not 0, while no job has been grabbed: a zero would read as a check
      // having just arrived.
      const age = /^gearman_worker_last_job_age_seconds (-?\d+)$/m.exec(text);
      expect(age).not.toBeNull();
      expect(Number(age![1])).toBe(-1);
    });
  });
});
