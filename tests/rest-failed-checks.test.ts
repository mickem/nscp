/**
 * The core's failed-check log, served by WEBServer.
 *
 * Every query the core dispatches - from any transport - that comes back
 * WARNING, CRITICAL or UNKNOWN is kept in a bounded in-memory log, and
 * /api/v2/failures serves it newest first:
 *
 *   GET    /api/v2/failures   list, with ?status= ?command= ?since= ?limit=   (failures.list)
 *   DELETE /api/v2/failures   empty the log                                   (failures.delete)
 *
 * CheckHelpers' check_ok / check_warning / check_critical give deterministic
 * outcomes, so the log's contents are exact. Two agents, because the
 * interesting behaviour is in [/settings/core/failed checks]: arguments are
 * not recorded unless asked for, and the cap evicts the oldest entries.
 */
import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(300_000);

type Json = Record<string, unknown>;

/** Baseline config; `log` is merged into /settings/core/failed checks. */
function config(log: Record<string, string>): Record<string, Record<string, string>> {
  return {
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
): Promise<{ nscp: NscpInstance; key: string }> {
  const nscp = new NscpInstance();
  await nscp.configure(config(log));
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
      expect(body.count).toEqual(0);
      expect(body.entries).toEqual([]);
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
});
