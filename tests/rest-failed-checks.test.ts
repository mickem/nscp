/**
 * The core's failed-check log, served by WEBServer.
 *
 * Every query the core dispatches - from any transport - that comes back
 * WARNING, CRITICAL or UNKNOWN is kept in a bounded in-memory log, and
 * /api/v2/failures serves it newest first:
 *
 *   GET    /api/v2/failures   list, with ?status= ?command= ?since= ?limit=   (failures.list)
 *   DELETE /api/v2/failures   empty the log                                   (failures.delete)
 *   GET    /api/v2/failures/<index>/context
 *                             the metrics snapshot it was recorded with      (failures.list)
 *
 * CheckHelpers' check_ok / check_warning / check_critical give deterministic
 * outcomes, so the log's contents are exact. Two agents, because the
 * interesting behaviour is in [/settings/core/failed checks]: arguments are
 * not recorded unless asked for, the cap evicts the oldest entries, and the
 * metrics snapshot is only kept when `capture metrics` is on.
 */
import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(300_000);

type Json = Record<string, unknown>;

/**
 * Baseline config; `log` is merged into /settings/core/failed checks and
 * `core` into /settings/core.
 */
function config(
  log: Record<string, string>,
  core: Record<string, string> = {},
): Record<string, Record<string, string>> {
  return {
    "/settings/core": core,
    "/modules": {
      WEBServer: "enabled",
      CheckHelpers: "enabled",
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/core/failed checks": log,
    "/settings/WEB/server/roles": {
      full: "*",
      // May read the log but not empty it, to prove the two grants are distinct.
      reader: "failures.list,login.get",
      // May run checks but has nothing to do with the log.
      runner: "queries.execute,login.get",
    },
    "/settings/WEB/server/users/admin": {
      role: "full",
      password: "default-password",
    },
    "/settings/WEB/server/users/reader": {
      role: "reader",
      password: "reader-password",
    },
    "/settings/WEB/server/users/runner": {
      role: "runner",
      password: "runner-password",
    },
  };
}

async function login(user: string, password: string): Promise<string> {
  const response = await request(REST_URL)
    .get("/api/v2/login")
    .auth(user, password)
    .trustLocalhost(true)
    .expect(200);
  return response.body.key as string;
}

/** Start an agent with the given log config and log in as admin. */
async function startAgent(
  log: Record<string, string>,
  core: Record<string, string> = {},
): Promise<{ nscp: NscpInstance; key: string }> {
  const nscp = new NscpInstance();
  await nscp.configure(config(log, core));
  await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  return { nscp, key: await login("admin", "default-password") };
}

/** Run one CheckHelpers check over REST and return its Nagios status. */
async function run(key: string, command: string, args = ""): Promise<number> {
  const response = await request(REST_URL)
    .get(`/api/v2/queries/${command}/commands/execute${args}`)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return (response.body as Json).result as number;
}

async function list(key: string, query = ""): Promise<Json> {
  const response = await request(REST_URL)
    .get(`/api/v2/failures${query}`)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return response.body as Json;
}

/** GET /api/v2/failures/<index>/context with the expected status. */
async function context(
  key: string,
  index: number | string,
  status = 200,
  query = "",
): Promise<Json> {
  const response = await request(REST_URL)
    .get(`/api/v2/failures/${index}/context${query}`)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(status);
  return response.body as Json;
}

async function clear(key: string): Promise<Json> {
  const response = await request(REST_URL)
    .delete("/api/v2/failures")
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return response.body as Json;
}

describe("REST failed-check log", () => {
  describe("defaults", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      // No keys at all: this is what a stock install looks like.
      ({ nscp, key } = await startAgent({}));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    beforeEach(async () => {
      // Each case starts from an empty log so counts are exact.
      await clear(key);
    });

    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/failures").trustLocalhost(true).expect(403);
      await request(REST_URL).delete("/api/v2/failures").trustLocalhost(true).expect(403);
    });

    it("is listed in the v2 endpoint index", async () => {
      const response = await request(REST_URL)
        .get("/api/v2")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect(response.body.failures_url).toMatch(/\/api\/v2\/failures$/);
    });

    it("is on by default and describes its configuration", async () => {
      const body = await list(key);
      expect(body.enabled).toBe(true);
      expect(body.max_entries).toEqual(50);
      expect(body.arguments_recorded).toBe(false);
      expect(body.statuses).toEqual("warning,critical,unknown");
      // Metrics context is opt-in.
      expect(body.capture_metrics).toBe(false);
      expect(body.context_bytes_used).toEqual(0);
      expect(body.context_budget as number).toBeGreaterThan(0);
      expect(body.count).toEqual(0);
      expect(body.entries).toEqual([]);
    });

    it("marks entries as recorded without metrics while capture is off", async () => {
      await run(key, "check_critical", "?message=no-context");
      const entry = ((await list(key)).entries as Json[])[0];
      expect(entry.context).toEqual({ state: "disabled" });

      const body = await context(key, entry.index as number);
      expect(body.state).toEqual("disabled");
      expect(body.index).toEqual(entry.index);
      expect(body.metrics).toBeUndefined();
    });

    it("answers 404 for an entry it does not hold and 400 for a malformed index", async () => {
      const body = await list(key);
      await context(key, (body.last_index as number) + 1000, 404);
      await context(key, 0, 400);
      await request(REST_URL)
        .get("/api/v2/failures/abc/context")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(404);
    });

    it("records a failed check with its result and perf data, not an OK one", async () => {
      expect(await run(key, "check_ok", "?message=fine")).toEqual(0);
      expect(await run(key, "check_critical", "?message=boom")).toEqual(2);

      const body = await list(key);
      expect(body.count).toEqual(1);
      const entries = body.entries as Json[];
      expect(entries).toHaveLength(1);
      const entry = entries[0];
      expect(entry.command).toEqual("check_critical");
      expect(entry.status).toEqual(2);
      expect(entry.result).toEqual("CRITICAL");
      expect(entry.message).toEqual("boom");
      expect(entry.depth).toEqual(0);
      // Dispatched by the web server on behalf of the logged-in user.
      expect(entry.subject).toEqual("WEBServer:admin");
      expect(typeof entry.index).toBe("number");
      expect(typeof entry.timestamp).toBe("number");
      expect(typeof entry.age).toBe("number");
      expect(typeof entry.date).toBe("string");
      const lines = entry.lines as Json[];
      expect(lines).toHaveLength(1);
      expect(lines[0].message).toEqual("boom");
      expect(typeof lines[0].perf).toBe("object");
      // Arguments are off by default: no key at all, not an empty list, so a
      // client can tell "none given" from "not recorded".
      expect(entry.arguments).toBeUndefined();
    });

    it("records what the core itself refuses, such as an unknown command", async () => {
      // No handler: the core answers UNKNOWN without asking any module.
      expect(await run(key, "no_such_check_xyz")).toEqual(3);

      const body = await list(key);
      const entries = body.entries as Json[];
      expect(entries).toHaveLength(1);
      expect(entries[0].command).toEqual("no_such_check_xyz");
      expect(entries[0].result).toEqual("UNKNOWN");
      expect(entries[0].message).toMatch(/no_such_check_xyz/);
    });

    it("lists newest first and filters by status and command", async () => {
      await run(key, "check_warning", "?message=first");
      await run(key, "check_critical", "?message=second");
      await run(key, "check_warning", "?message=third");

      const all = (await list(key)).entries as Json[];
      expect(all.map((e) => e.message)).toEqual(["third", "second", "first"]);
      // Strictly increasing indices, newest first.
      expect((all[0].index as number) > (all[1].index as number)).toBe(true);
      expect((all[1].index as number) > (all[2].index as number)).toBe(true);

      const warnings = (await list(key, "?status=warning")).entries as Json[];
      expect(warnings.map((e) => e.message)).toEqual(["third", "first"]);
      // Numbers are the same vocabulary as words.
      const criticals = (await list(key, "?status=2")).entries as Json[];
      expect(criticals.map((e) => e.message)).toEqual(["second"]);

      const byCommand = (await list(key, "?command=check_critical")).entries as Json[];
      expect(byCommand.map((e) => e.message)).toEqual(["second"]);

      // limit applies after the filter: "the latest warning", not "the
      // warnings among the latest one".
      const latestWarning = (await list(key, "?status=warning&limit=1")).entries as Json[];
      expect(latestWarning.map((e) => e.message)).toEqual(["third"]);
    });

    it("pages with since, and rejects a malformed filter", async () => {
      await run(key, "check_critical", "?message=older");
      const first = (await list(key)).entries as Json[];
      const seen = first[0].index as number;

      await run(key, "check_critical", "?message=newer");
      const body = await list(key, `?since=${seen}`);
      expect((body.entries as Json[]).map((e) => e.message)).toEqual(["newer"]);
      expect(body.last_index).toEqual(seen + 1);

      const auth = { Authorization: `Bearer ${key}` };
      await request(REST_URL)
        .get("/api/v2/failures?since=abc")
        .set(auth)
        .trustLocalhost(true)
        .expect(400);
      await request(REST_URL)
        .get("/api/v2/failures?limit=-1")
        .set(auth)
        .trustLocalhost(true)
        .expect(400);
      await request(REST_URL)
        .get("/api/v2/failures?status=bogus")
        .set(auth)
        .trustLocalhost(true)
        .expect(400);
    });

    it("keeps the grants distinct: a reader cannot empty the log", async () => {
      await run(key, "check_critical", "?message=kept");
      const reader = await login("reader", "reader-password");
      const body = await list(reader);
      expect(body.count).toEqual(1);
      await request(REST_URL)
        .delete("/api/v2/failures")
        .set("Authorization", `Bearer ${reader}`)
        .trustLocalhost(true)
        .expect(403);
      // Still there.
      expect((await list(key)).count).toEqual(1);
      // Reading an entry's context is part of reading the log.
      const index = ((await list(key)).entries as Json[])[0].index as number;
      await context(reader, index);
    });

    it("records failures from any caller, and hides the log from a caller without the grant", async () => {
      const runner = await login("runner", "runner-password");
      expect(await run(runner, "check_critical", "?message=from-runner")).toEqual(2);
      await request(REST_URL)
        .get("/api/v2/failures")
        .set("Authorization", `Bearer ${runner}`)
        .trustLocalhost(true)
        .expect(403);

      const entries = (await list(key)).entries as Json[];
      expect(entries).toHaveLength(1);
      expect(entries[0].subject).toEqual("WEBServer:runner");
      await context(runner, entries[0].index as number, 403);
    });

    it("empties the log on DELETE and reports how many went", async () => {
      await run(key, "check_critical", "?message=a");
      await run(key, "check_warning", "?message=b");
      const body = await clear(key);
      expect(body.removed).toEqual(2);
      expect(body.enabled).toBe(true);
      expect((await list(key)).count).toEqual(0);
    });
  });

  describe("record arguments = true, max entries = 3, statuses = critical,unknown", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      ({ nscp, key } = await startAgent({
        "record arguments": "true",
        "max entries": "3",
        statuses: "critical,unknown",
      }));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    beforeEach(async () => {
      await clear(key);
    });

    it("describes the configuration", async () => {
      const body = await list(key);
      expect(body.max_entries).toEqual(3);
      expect(body.arguments_recorded).toBe(true);
      expect(body.statuses).toEqual("critical,unknown");
    });

    it("records the arguments and redacts the sensitive ones", async () => {
      // check_critical does not take these options, so this is either a
      // CRITICAL or an UNKNOWN (unrecognised option) - both are recorded here,
      // and the arguments, not the verdict, are what this case is about.
      await run(key, "check_critical", "?message=boom&password=hunter2&token=abc&user=bob");

      const entries = (await list(key)).entries as Json[];
      expect(entries).toHaveLength(1);
      const args = entries[0].arguments as string[];
      expect(args).toContain("message=boom");
      expect(args).toContain("user=bob");
      expect(args).toContain("password=***");
      expect(args).toContain("token=***");
      expect(args.join(" ")).not.toContain("hunter2");
      expect(args.join(" ")).not.toContain("abc");
    });

    it("only records the configured statuses", async () => {
      expect(await run(key, "check_warning", "?message=ignored")).toEqual(1);
      expect(await run(key, "check_critical", "?message=kept")).toEqual(2);
      const entries = (await list(key)).entries as Json[];
      expect(entries.map((e) => e.message)).toEqual(["kept"]);
    });

    it("evicts the oldest entries past the cap and counts them", async () => {
      for (const n of [1, 2, 3, 4, 5]) {
        await run(key, "check_critical", `?message=n${n}`);
      }
      const body = await list(key);
      expect(body.count).toEqual(3);
      expect((body.entries as Json[]).map((e) => e.message)).toEqual(["n5", "n4", "n3"]);
      expect(body.dropped as number).toBeGreaterThanOrEqual(2);
      // The index keeps counting past what was evicted.
      expect(body.last_index as number).toBeGreaterThanOrEqual(5);
    });
  });

  describe("capture metrics = true", () => {
    let nscp: NscpInstance;
    let key = "";

    /** Wait until the core has collected a snapshot: the first failure after that carries one. */
    async function captured(message: string): Promise<Json> {
      const deadline = Date.now() + 30_000;
      for (;;) {
        await clear(key);
        await run(key, "check_critical", `?message=${message}`);
        const entry = ((await list(key)).entries as Json[])[0];
        if ((entry.context as Json).state === "captured" || Date.now() > deadline) return entry;
        await new Promise((resolve) => setTimeout(resolve, 500));
      }
    }

    beforeAll(async () => {
      // A short metrics interval so the first snapshot lands quickly.
      ({ nscp, key } = await startAgent(
        { "capture metrics": "true" },
        { "metrics interval": "1s" },
      ));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("describes the configuration", async () => {
      const body = await list(key);
      expect(body.capture_metrics).toBe(true);
    });

    it("records each failure with the latest metrics snapshot", async () => {
      const entry = await captured("with-context");
      const ref = entry.context as Json;
      expect(ref.state).toEqual("captured");
      expect(typeof ref.snapshot_id).toBe("number");
      // The snapshot is from before the failure, by at most a few intervals.
      expect(ref.captured_at as number).toBeLessThanOrEqual(entry.timestamp as number);
      expect(ref.age_at_failure as number).toBeLessThanOrEqual(5);
      // The listing references the snapshot; it does not carry it.
      expect(ref.metrics).toBeUndefined();

      const body = await context(key, entry.index as number);
      expect(body.state).toEqual("captured");
      expect(body.snapshot_id).toEqual(ref.snapshot_id);
      const metrics = body.metrics as Record<string, unknown>;
      // The core's own bundle is always there, whatever modules are loaded.
      const keys = Object.keys(metrics);
      expect(keys.some((k) => k.startsWith("workers."))).toBe(true);
      // Keyed exactly as /api/v2/metrics keys them, with the metadata alongside.
      const metadata = body.metadata as Record<string, Json>;
      for (const k of keys) expect(metadata[k]).toBeDefined();
      const bare = await context(key, entry.index as number, 200, "?meta=0");
      expect(bare.metadata).toBeUndefined();
      expect(Object.keys(bare.metrics as Json)).toEqual(keys);

      const envelope = await list(key);
      expect(envelope.context_bytes_used as number).toBeGreaterThan(0);
    });

    it("shares one snapshot between failures in the same interval", async () => {
      await captured("first");
      await run(key, "check_warning", "?message=second");
      const entries = (await list(key)).entries as Json[];
      expect(entries).toHaveLength(2);
      const [second, first] = entries.map((e) => e.context as Json);
      // Back to back, so almost always the same one; a metrics round landing in
      // between makes it the next one. Either way the budget counts what is held.
      expect((second.snapshot_id as number) - (first.snapshot_id as number)).toBeLessThanOrEqual(1);
      const envelope = await list(key);
      if (second.snapshot_id === first.snapshot_id) {
        expect(envelope.context_bytes_used).toEqual(first.size);
      }
    });

    it("releases the snapshots when the log is cleared", async () => {
      await captured("to-clear");
      await clear(key);
      expect((await list(key)).context_bytes_used).toEqual(0);
    });
  });

  describe("prune metrics = zeros, empty, duplicates, prune pattern = workers.threads", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      ({ nscp, key } = await startAgent(
        {
          "capture metrics": "true",
          "prune metrics": "zeros, empty, duplicates",
          "prune pattern": "WORKERS.threads",
        },
        { "metrics interval": "1s" },
      ));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("keeps no zero or empty value and says what it left out", async () => {
      const deadline = Date.now() + 30_000;
      let entry: Json;
      for (;;) {
        await clear(key);
        await run(key, "check_critical", "?message=pruned");
        entry = ((await list(key)).entries as Json[])[0];
        if ((entry.context as Json).state === "captured" || Date.now() > deadline) break;
        await new Promise((resolve) => setTimeout(resolve, 500));
      }
      // The listing never carries the pruning summary, only the reference.
      expect((entry.context as Json).pruned).toBeUndefined();

      const body = await context(key, entry.index as number);
      expect(body.state).toEqual("captured");
      const metrics = body.metrics as Record<string, unknown>;
      expect(Object.keys(metrics).length).toBeGreaterThan(0);
      for (const [k, v] of Object.entries(metrics)) {
        expect([k, v]).not.toEqual([k, 0]);
        expect([k, v]).not.toEqual([k, ""]);
      }
      // Every key left still has its metadata.
      const metadata = body.metadata as Record<string, Json>;
      for (const k of Object.keys(metrics)) expect(metadata[k]).toBeDefined();

      const pruned = body.pruned as Json;
      expect(pruned.rules).toEqual("zeros,empty,duplicates");
      expect(typeof pruned.zeros).toBe("number");
      expect(typeof pruned.empty).toBe("number");
      expect(typeof pruned.duplicates).toBe("number");
      // The core always publishes workers.threads (and never as 0: a pool runs
      // the checks), so the pattern is what removed it.
      expect(pruned.patterns).toEqual(["workers.threads"]);
      expect(pruned.excluded).toBeGreaterThanOrEqual(1);
      expect(metrics["workers.threads"]).toBeUndefined();
      // A collapsed instance is gone from the snapshot; the one it matched is not.
      for (const [dropped, kept] of Object.entries(pruned.collapsed as Record<string, string>)) {
        expect(Object.keys(metrics).some((k) => k.startsWith(`${dropped}.`))).toBe(false);
        expect(dropped).not.toEqual(kept);
      }
    });
  });
});
