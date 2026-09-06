/**
 * The WEB server's passive-result cache.
 *
 * WEBServer registers a submission channel and keeps whatever is submitted to
 * it, so a monitoring system that cannot reach the agent can poll results out
 * of it instead of the agent pushing them. This exercises the whole path for
 * real: CheckHelpers' check_and_forward submits a result on the configured
 * channel, and the results_controller endpoints then have to show it.
 *
 *   GET    /api/v2/results        list, optionally filtered   (results.list)
 *   GET    /api/v2/results/{key}  one cached result           (results.get)
 *   DELETE /api/v2/results        drop everything cached      (results.delete)
 *   DELETE /api/v2/results/{key}  drop one cached result      (results.delete)
 */
import request from "supertest";

import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(120_000);

// The channel and key layout the agent is configured with below. The key is
// pinned to a literal prefix so the assertions do not depend on whatever
// ${hostname} resolves to on the test machine.
const CHANNEL = "WEBCACHE";

describe("REST passive result cache", () => {
  let nscp: NscpInstance;
  let key = "";

  /** Run a query over REST and hand back the JSON result payload. */
  async function runQuery(command: string, query = ""): Promise<Record<string, unknown>> {
    const response = await request(REST_URL)
      .get(`/api/v2/queries/${command}/commands/execute${query}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    return response.body as Record<string, unknown>;
  }

  /** Submit one passive result on the cached channel. */
  async function submit(command: string, alias: string): Promise<void> {
    const result = await runQuery(
      "check_and_forward",
      `?command=${command}&channel=${CHANNEL}&alias=${alias}`,
    );
    expect(result.result).toEqual(0); // 0 = OK, i.e. the submission went through
  }

  async function listResults(query = ""): Promise<Record<string, unknown>[]> {
    const response = await request(REST_URL)
      .get(`/api/v2/results${query}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    return response.body as Record<string, unknown>[];
  }

  /** The one cached entry whose alias matches, or undefined. */
  function byAlias(
    results: Record<string, unknown>[],
    alias: string,
  ): Record<string, unknown> | undefined {
    return results.find((r) => r.alias === alias);
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckHelpers: "enabled",
      },
      "/settings/default": {
        password: "default-password",
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/results": {
        channel: CHANNEL,
        "primary index": "cached/${alias-or-command}",
      },
      "/settings/WEB/server/roles": {
        full: "*",
        // A caller that may read the cache but not empty it, to prove the
        // three privileges are actually distinct.
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

  beforeEach(async () => {
    // Each case starts from an empty cache so counts are exact.
    await request(REST_URL)
      .delete("/api/v2/results")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
  });

  it("is empty until something is submitted on the channel", async () => {
    expect(await listResults()).toEqual([]);
  });

  it("caches a result submitted on the configured channel", async () => {
    await submit("check_critical", "disk");

    const results = await listResults();
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
    expect(typeof entry.age).toBe("number");
    expect(entry.result_url).toMatch(/\/api\/v2\/results\/cached\/disk$/);
  });

  it("records the submitting host the header names", async () => {
    // check_and_forward's `source` becomes the header's sender, which is how
    // a relayed result identifies the machine it is actually about.
    const result = await runQuery(
      "check_and_forward",
      `?command=check_ok&channel=${CHANNEL}&alias=relayed&source=remote-agent`,
    );
    expect(result.result).toEqual(0);

    const results = await listResults();
    expect(results).toHaveLength(1);
    expect(results[0].host).toEqual("remote-agent");
    expect(results[0].source).toEqual("remote-agent");
    // ...and the filter agrees with the field.
    expect(await listResults("?host=remote-agent")).toHaveLength(1);
    expect(await listResults("?host=someone-else")).toEqual([]);
  });

  it("replaces rather than accumulates when a key reports again", async () => {
    await submit("check_ok", "flapper");
    await submit("check_critical", "flapper");

    const results = await listResults();
    expect(results).toHaveLength(1);
    // Last result wins, and the key remembers how often it has reported.
    expect(results[0].status).toEqual(2);
    expect(results[0].count).toEqual(2);
  });

  it("keeps distinct keys side by side, sorted by key", async () => {
    await submit("check_ok", "zulu");
    await submit("check_warning", "alpha");

    const results = await listResults();
    expect(results.map((r) => r.key)).toEqual(["cached/alpha", "cached/zulu"]);
  });

  it("filters by status, exactly and by name or number", async () => {
    await submit("check_ok", "fine");
    await submit("check_critical", "broken");

    expect((await listResults("?status=critical")).map((r) => r.alias)).toEqual(["broken"]);
    expect((await listResults("?status=2")).map((r) => r.alias)).toEqual(["broken"]);
    const both = await listResults("?status=ok,critical");
    expect(both).toHaveLength(2);
  });

  it("filters by alias, command and channel", async () => {
    await submit("check_ok", "fine");
    await submit("check_critical", "broken");

    expect((await listResults("?alias=fine")).map((r) => r.alias)).toEqual(["fine"]);
    expect(await listResults("?command=check_critical")).toHaveLength(1);
    expect(await listResults(`?channel=${CHANNEL}`)).toHaveLength(2);
    // A channel nothing was submitted on matches nothing rather than everything.
    expect(await listResults("?channel=NSCA")).toEqual([]);
  });

  it("rejects an unrecognised status filter instead of ignoring it", async () => {
    await request(REST_URL)
      .get("/api/v2/results?status=sideways")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(400);
  });

  it("fetches a single result by its key, slashes and all", async () => {
    await submit("check_warning", "single");

    const response = await request(REST_URL)
      .get("/api/v2/results/cached/single")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.key).toEqual("cached/single");
    expect(response.body.status).toEqual(1);
  });

  it("answers 404 for a key that was never cached", async () => {
    await request(REST_URL)
      .get("/api/v2/results/cached/nonesuch")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(404);
  });

  it("deletes a single result and leaves the rest alone", async () => {
    await submit("check_ok", "keepme");
    await submit("check_ok", "dropme");

    await request(REST_URL)
      .delete("/api/v2/results/cached/dropme")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => expect(response.body.removed).toEqual(1));

    expect((await listResults()).map((r) => r.key)).toEqual(["cached/keepme"]);

    // A second delete of the same key has nothing to remove.
    await request(REST_URL)
      .delete("/api/v2/results/cached/dropme")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(404);
  });

  it("empties the cache and reports how much it dropped", async () => {
    await submit("check_ok", "one");
    await submit("check_ok", "two");

    await request(REST_URL)
      .delete("/api/v2/results")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => expect(response.body.removed).toEqual(2));

    expect(await listResults()).toEqual([]);
  });

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/results").trustLocalhost(true).expect(403);
    await request(REST_URL).get("/api/v2/results/cached/x").trustLocalhost(true).expect(403);
    await request(REST_URL).delete("/api/v2/results").trustLocalhost(true).expect(403);
  });

  it("separates reading the cache from emptying it", async () => {
    await submit("check_ok", "guarded");

    // The `reader` role holds results.list / results.get but not
    // results.delete, so it may look and not touch.
    await request(REST_URL)
      .get("/api/v2/results")
      .auth("reader", "reader-password")
      .trustLocalhost(true)
      .expect(200);
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
    expect(await listResults()).toHaveLength(1);
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
    await submit("check_ok", "v1");

    const response = await request(REST_URL)
      .get("/api/v1/results")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect((response.body as Record<string, unknown>[]).map((r) => r.key)).toEqual(["cached/v1"]);
  });
});
