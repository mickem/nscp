/**
 * REST host-facts scenarios: /api/v2/facts serves the inventory the core
 * collects from its producers.
 *
 * Distinct from rest-tags.test.ts next door, and the split is the point. A tag
 * is a flat value a fleet selector matches whole and is always published; a
 * fact set is a document and is collected only once an operator enables it in
 * the module that produces it. CheckSystem is the producer here, with `os` and
 * `hardware` turned on.
 */
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";
const factsSection = `/settings/system/${onWindows ? "windows" : "unix"}/facts`;

describe("REST facts", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckSystem: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      [factsSection]: {
        os: "true",
        hardware: "true",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/facts").trustLocalhost(true).expect(403);
  });

  it("is listed in the v2 endpoint index", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts_url).toMatch(/\/api\/v2\/facts$/);
      });
  });

  it("serves the sets CheckSystem was configured to produce", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.enabled.sort()).toEqual(["hardware", "os"]);
        expect(response.body.errors).toEqual({});
        expect(response.body.found).toBe(true);
        expect(response.body.revision).toBeGreaterThan(0);
        // ISO 8601 UTC, as the document rules require.
        expect(response.body.collected).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
        // Per set, when its values were actually read off the machine.
        // CheckSystem caches, so this is what says how old the numbers are -
        // `collected` only says when the core last asked.
        expect(Object.keys(response.body.gathered).sort()).toEqual(["hardware", "os"]);
        expect(response.body.gathered.os).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);

        expect(response.body.facts.os.family).toEqual(onWindows ? "windows" : "linux");
        expect(response.body.facts.os.name).toBeTruthy();
        expect(response.body.facts.os.version).toBeTruthy();
        expect(response.body.facts.os.arch).toMatch(/^[a-z0-9_]+$/);
        // Numbers cross the wire as numbers here, unlike in the string-valued
        // tag map - that is half the reason the document exists.
        expect(typeof response.body.facts.hardware.cpu_cores).toBe("number");
        expect(response.body.facts.hardware.cpu_cores).toBeGreaterThan(0);
        expect(typeof response.body.facts.hardware.memory_gb).toBe("number");
      });
  });

  it("serves a subtree, and says so when nothing produced one", async () => {
    await request(REST_URL)
      .get("/api/v2/facts?path=os.family")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.path).toEqual("os.family");
        expect(response.body.found).toBe(true);
        expect(response.body.facts).toEqual(onWindows ? "windows" : "linux");
      });

    // A set nobody produces is not an error: a UI asking for one an operator
    // has not enabled should render "not collected", not a failure.
    await request(REST_URL)
      .get("/api/v2/facts?path=storage")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.found).toBe(false);
        expect(response.body.facts).toEqual({});
      });
  });

  it("collects on demand, and that is what moves the gathered time", async () => {
    const before = await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);

    const refreshed = await request(REST_URL)
      .post("/api/v2/facts/commands/refresh")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);

    // The round answers with the document it produced, so a caller does not
    // have to follow up with a GET.
    expect(refreshed.body.facts.os.family).toEqual(onWindows ? "windows" : "linux");
    // A manual refresh is the reason that makes a cached producer read the
    // machine again, so the values are at least as fresh as they were.
    expect(new Date(refreshed.body.gathered.os).getTime()).toBeGreaterThanOrEqual(
      new Date(before.body.gathered.os).getTime(),
    );
    // Nothing about this host changed, so the document did not either.
    expect(refreshed.body.revision).toEqual(before.body.revision);
  });
});

describe("REST facts with no set enabled", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckSystem: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("reports an empty inventory rather than an error", async () => {
    // The default: an inventory is data an operator did not necessarily agree
    // to ship, so a fresh install collects nothing and says so plainly.
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.enabled).toEqual([]);
        expect(response.body.facts).toEqual({});
        // Nothing was stored, so the revision never moved off its initial 0.
        // `collected` is when the last round *completed*, and a round with no
        // producer to ask completes like any other - so it carries a
        // timestamp even here.
        expect(response.body.revision).toEqual(0);
      });
  });
});
