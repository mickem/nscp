/**
 * The WEB server's passive-result cache.
 *
 * WEBServer can register a submission channel and keep whatever is submitted
 * to it, so a monitoring system that cannot reach the agent can poll results
 * out of it instead of the agent pushing them. This exercises the whole path
 * for real: CheckHelpers' check_and_forward submits a result on the configured
 * channel, and the results_controller endpoints then have to show it.
 *
 *   GET    /api/v2/results        poll, optionally filtered   (results.list)
 *   GET    /api/v2/results/{key}  one cached result           (results.get)
 *   DELETE /api/v2/results        drop everything cached      (results.delete)
 *   DELETE /api/v2/results/{key}  drop one cached result      (results.delete)
 *
 * Three agents are configured, because the interesting behaviour is in the
 * settings: the cache is off by default, `mode` decides which of two results
 * for a key survives, and `clear on poll` decides whether a poll consumes what
 * it reports.
 */
import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(300_000);

const CHANNEL = "WEBCACHE";

/** Baseline config; `results` is merged into /settings/WEB/server/results. */
function config(results: Record<string, string>): Record<string, Record<string, string>> {
  return {
    "/modules": {
      WEBServer: "enabled",
      CheckHelpers: "enabled",
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/WEB/server/results": results,
    "/settings/WEB/server/roles": {
      full: "*",
      // A caller that may read the cache but not empty it, to prove the three
      // privileges are actually distinct.
      reader: "results.list,results.get,login.get",
    },
    "/settings/WEB/server/users/admin": {
      role: "full",
      password: "default-password",
    },
    "/settings/WEB/server/users/reader": {
      role: "reader",
      password: "reader-password",
    },
  };
}

/** Start an agent with the given results config and log in as admin. */
async function startAgent(
  results: Record<string, string>,
): Promise<{ nscp: NscpInstance; key: string }> {
  const nscp = new NscpInstance();
  await nscp.configure(config(results));
  await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(8443, { timeoutMs: 30_000 });

  const login = await request(REST_URL)
    .get("/api/v2/login")
    .auth("admin", "default-password")
    .trustLocalhost(true)
    .expect(200);
  return { nscp, key: login.body.key as string };
}

/** Submit one passive result on the cached channel via check_and_forward. */
async function submit(key: string, command: string, alias: string, extra = ""): Promise<void> {
  const response = await request(REST_URL)
    .get(
      `/api/v2/queries/check_and_forward/commands/execute?command=${command}&channel=${CHANNEL}&alias=${encodeURIComponent(alias)}${extra}`,
    )
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  // 0 = OK, i.e. the submission itself went through.
  expect((response.body as Record<string, unknown>).result).toEqual(0);
}

async function poll(key: string, query = ""): Promise<Record<string, unknown>[]> {
  const response = await request(REST_URL)
    .get(`/api/v2/results${query}`)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  return response.body as Record<string, unknown>[];
}

describe("REST passive result cache", () => {
  describe("disabled (the default)", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      // No `enabled` key at all: this is what a stock install looks like.
      ({ nscp, key } = await startAgent({ channel: CHANNEL }));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("answers 503 rather than an empty list, so 'off' is distinguishable", async () => {
      const response = await request(REST_URL)
        .get("/api/v2/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(503);
      expect(response.text).toMatch(/disabled/i);
      expect(response.text).toMatch(/enabled/);
    });

    it("answers 503 on every route", async () => {
      const auth = { Authorization: `Bearer ${key}` };
      await request(REST_URL).get("/api/v2/results/x").set(auth).trustLocalhost(true).expect(503);
      await request(REST_URL).delete("/api/v2/results").set(auth).trustLocalhost(true).expect(503);
      await request(REST_URL)
        .delete("/api/v2/results/x")
        .set(auth)
        .trustLocalhost(true)
        .expect(503);
    });

    it("still checks authentication first, so the state is not a probe oracle", async () => {
      await request(REST_URL).get("/api/v2/results").trustLocalhost(true).expect(403);
    });

    it("registers no channel, so a submission is refused rather than swallowed", async () => {
      const response = await request(REST_URL)
        .get(
          `/api/v2/queries/check_and_forward/commands/execute?command=check_ok&channel=${CHANNEL}&alias=nope`,
        )
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      // 3 = UNKNOWN: the core has no handler for the channel.
      expect((response.body as Record<string, unknown>).result).toEqual(3);
    });
  });

  describe("enabled, mode=last, clear on poll", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      ({ nscp, key } = await startAgent({
        enabled: "true",
        channel: CHANNEL,
        "primary index": "cached/${alias-or-command}",
      }));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    beforeEach(async () => {
      // Each case starts from an empty cache so counts are exact.
      await request(REST_URL)
        .delete("/api/v2/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
    });

    it("is empty until something is submitted on the channel", async () => {
      expect(await poll(key)).toEqual([]);
    });

    it("caches a result submitted on the configured channel", async () => {
      await submit(key, "check_critical", "disk");

      const results = await poll(key);
      expect(results).toHaveLength(1);
      const entry = results[0];
      expect(entry.key).toEqual("cached/disk");
      expect(entry.channel).toEqual(CHANNEL);
      expect(entry.alias).toEqual("disk");
      expect(entry.command).toEqual("check_critical");
      expect(entry.status).toEqual(2);
      expect(entry.result).toEqual("CRITICAL");
      expect(typeof entry.message).toBe("string");
      expect(entry.message).not.toEqual("");
      // The store stamps these, so a consumer always has them.
      expect(entry.count).toEqual(1);
      expect(typeof entry.host).toBe("string");
      expect(entry.host).not.toEqual("");
      expect(typeof entry.last_seen).toBe("number");
      expect(entry.result_seen).toEqual(entry.last_seen);
      expect(typeof entry.age).toBe("number");
      expect(entry.result_url).toMatch(/\/api\/v2\/results\/cached\/disk$/);
    });

    it("records the submitting host the header names", async () => {
      // check_and_forward's `source` becomes the header's sender, which is how
      // a relayed result identifies the machine it is actually about.
      await submit(key, "check_ok", "relayed", "&source=remote-agent");

      const results = await poll(key, "?host=remote-agent");
      expect(results).toHaveLength(1);
      expect(results[0].host).toEqual("remote-agent");
      expect(results[0].source).toEqual("remote-agent");
    });

    it("a poll consumes what it reports", async () => {
      await submit(key, "check_ok", "consumed");
      expect(await poll(key)).toHaveLength(1);
      // Second poll: nothing has happened since the first one.
      expect(await poll(key)).toEqual([]);
      expect(await poll(key)).toEqual([]);
    });

    it("a filtered poll keeps what it did not return", async () => {
      await submit(key, "check_critical", "broken");
      await submit(key, "check_ok", "fine");

      const problems = await poll(key, "?status=critical");
      expect(problems.map((r) => r.alias)).toEqual(["broken"]);
      // The OK was never shown to the caller, so it must still be there.
      expect((await poll(key)).map((r) => r.alias)).toEqual(["fine"]);
    });

    it("in last mode a recovery replaces the problem before it", async () => {
      await submit(key, "check_critical", "flapper");
      await submit(key, "check_ok", "flapper");

      const results = await poll(key);
      expect(results).toHaveLength(1);
      expect(results[0].status).toEqual(0);
      // ...and the key remembers that it reported twice.
      expect(results[0].count).toEqual(2);
    });

    it("keeps distinct keys side by side, sorted by key", async () => {
      await submit(key, "check_ok", "zulu");
      await submit(key, "check_warning", "alpha");

      expect((await poll(key)).map((r) => r.key)).toEqual(["cached/alpha", "cached/zulu"]);
    });

    it("filters by status, exactly and by name or number", async () => {
      await submit(key, "check_ok", "fine");
      await submit(key, "check_critical", "broken");
      expect((await poll(key, "?status=critical")).map((r) => r.alias)).toEqual(["broken"]);

      await submit(key, "check_critical", "broken");
      expect((await poll(key, "?status=2")).map((r) => r.alias)).toEqual(["broken"]);

      await submit(key, "check_critical", "broken");
      expect(await poll(key, "?status=ok,critical")).toHaveLength(2);
    });

    it("filters by alias, command and channel", async () => {
      await submit(key, "check_ok", "fine");
      await submit(key, "check_critical", "broken");

      expect((await poll(key, "?alias=fine")).map((r) => r.alias)).toEqual(["fine"]);
      expect(await poll(key, "?command=check_critical")).toHaveLength(1);

      // Every poll above drained what it matched, so re-stock before testing
      // the channel filter - otherwise both assertions below pass on an empty
      // cache and prove nothing.
      await submit(key, "check_ok", "fine");
      // A channel nothing was submitted on matches nothing rather than everything...
      expect(await poll(key, "?channel=NSCA")).toEqual([]);
      // ...while the channel it actually arrived on matches.
      expect((await poll(key, `?channel=${CHANNEL}`)).map((r) => r.alias)).toEqual(["fine"]);
    });

    it("round-trips a key that has to be escaped in a URL", async () => {
      // An alias is operator-defined text and routinely holds a space, so the
      // URL the cache advertises for it has to be one a client can fetch.
      await submit(key, "check_ok", "disk C");

      const listed = await poll(key);
      expect(listed).toHaveLength(1);
      expect(listed[0].key).toEqual("cached/disk C");
      const path = new URL(String(listed[0].result_url)).pathname;
      expect(path).toEqual("/api/v2/results/cached/disk%20C");

      // The poll consumed it; put it back and fetch it by the advertised URL.
      await submit(key, "check_ok", "disk C");
      const single = await request(REST_URL)
        .get(path)
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect(single.body.key).toEqual("cached/disk C");

      // ...and the same URL deletes it.
      await request(REST_URL)
        .delete(path)
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect(await poll(key)).toEqual([]);
    });

    it("rejects an unrecognised status filter instead of ignoring it", async () => {
      await request(REST_URL)
        .get("/api/v2/results?status=sideways")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(400);
    });

    it("fetches a single result by key without consuming it", async () => {
      await submit(key, "check_warning", "single");

      // Twice: a lookup is not a poll, so the second one still finds it.
      for (let attempt = 0; attempt < 2; attempt++) {
        const response = await request(REST_URL)
          .get("/api/v2/results/cached/single")
          .set("Authorization", `Bearer ${key}`)
          .trustLocalhost(true)
          .expect(200);
        expect(response.body.key).toEqual("cached/single");
        expect(response.body.status).toEqual(1);
      }
      // ...and the poll still gets it.
      expect(await poll(key)).toHaveLength(1);
    });

    it("answers 404 for a key that was never cached", async () => {
      await request(REST_URL)
        .get("/api/v2/results/cached/nonesuch")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(404);
    });

    it("deletes a single result and leaves the rest alone", async () => {
      await submit(key, "check_ok", "keepme");
      await submit(key, "check_ok", "dropme");

      await request(REST_URL)
        .delete("/api/v2/results/cached/dropme")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => expect(response.body.removed).toEqual(1));

      expect((await poll(key)).map((r) => r.key)).toEqual(["cached/keepme"]);

      // A second delete of the same key has nothing to remove.
      await request(REST_URL)
        .delete("/api/v2/results/cached/dropme")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(404);
    });

    it("empties the cache and reports how much it dropped", async () => {
      await submit(key, "check_ok", "one");
      await submit(key, "check_ok", "two");

      await request(REST_URL)
        .delete("/api/v2/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => expect(response.body.removed).toEqual(2));

      expect(await poll(key)).toEqual([]);
    });

    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/results").trustLocalhost(true).expect(403);
      await request(REST_URL).get("/api/v2/results/cached/x").trustLocalhost(true).expect(403);
      await request(REST_URL).delete("/api/v2/results").trustLocalhost(true).expect(403);
    });

    it("separates reading the cache from emptying it", async () => {
      await submit(key, "check_ok", "guarded");

      // The `reader` role holds results.list / results.get but not
      // results.delete, so it may look and not touch.
      await request(REST_URL)
        .get("/api/v2/results/cached/guarded")
        .auth("reader", "reader-password")
        .trustLocalhost(true)
        .expect(200);
      await request(REST_URL)
        .delete("/api/v2/results")
        .auth("reader", "reader-password")
        .trustLocalhost(true)
        .expect(403);

      // ...and the refused delete really did not empty it.
      expect(await poll(key)).toHaveLength(1);
    });

    it("advertises the endpoint from the API index", async () => {
      const response = await request(REST_URL)
        .get("/api/v2")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect(response.body.results_url).toMatch(/\/api\/v2\/results$/);
    });

    it("is also mounted on the v1 prefix", async () => {
      await submit(key, "check_ok", "v1");

      const response = await request(REST_URL)
        .get("/api/v1/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
      expect((response.body as Record<string, unknown>[]).map((r) => r.key)).toEqual(["cached/v1"]);
    });
  });

  describe("enabled, mode=worst, no clear on poll", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      ({ nscp, key } = await startAgent({
        enabled: "true",
        channel: CHANNEL,
        mode: "worst",
        "clear on poll": "false",
        "primary index": "cached/${alias-or-command}",
      }));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    beforeEach(async () => {
      await request(REST_URL)
        .delete("/api/v2/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);
    });

    it("holds a problem through a recovery", async () => {
      // The reason the mode exists: a CRITICAL that recovers before the next
      // poll must still be reported, not silently overwritten by the OK.
      await submit(key, "check_critical", "flapper");
      await submit(key, "check_ok", "flapper");

      const results = await poll(key);
      expect(results).toHaveLength(1);
      expect(results[0].status).toEqual(2);
      // The suppressed OK still counts as a report, so the check does not look
      // like it stopped running...
      expect(results[0].count).toEqual(2);
      // ...while the retained problem stays dated to when it happened.
      expect(Number(results[0].result_seen)).toBeLessThanOrEqual(Number(results[0].last_seen));
    });

    it("still escalates to a worse result", async () => {
      await submit(key, "check_warning", "rising");
      await submit(key, "check_critical", "rising");

      expect((await poll(key))[0].status).toEqual(2);
    });

    it("does not consume what it reports when clear on poll is off", async () => {
      await submit(key, "check_ok", "sticky");

      expect(await poll(key)).toHaveLength(1);
      expect(await poll(key)).toHaveLength(1);
      expect(await poll(key)).toHaveLength(1);
    });

    it("clears the held worst result once it is deleted", async () => {
      // "Worst" means worst since the cache was last emptied, not worst ever.
      await submit(key, "check_critical", "reset");
      expect((await poll(key))[0].status).toEqual(2);

      await request(REST_URL)
        .delete("/api/v2/results")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200);

      await submit(key, "check_ok", "reset");
      expect((await poll(key))[0].status).toEqual(0);
    });
  });
});
