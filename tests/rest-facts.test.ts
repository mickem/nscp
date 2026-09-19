/**
 * REST host-facts scenarios: /api/v2/facts serves the fact repository — the
 * structured, hierarchical inventory modules publish through fetchFacts, as
 * opposed to /api/v2/tags, which serves the flat strings a fleet selector
 * matches on.
 *
 * The contract this pins is the opt-in one: with nothing enabled in
 * [/settings/facts] the endpoint answers with an empty document, an empty
 * enabled list, and the hash of `{}` — it does not 404, and it does not
 * quietly collect anything. That is the state every fresh install is in, so
 * it is the state most worth asserting.
 *
 * Grants: reading is `facts.get` (which the `monitoring` role carries) and
 * refreshing is `facts.refresh` (which only `full` does), because a refresh
 * makes every producer collect now.
 *
 * Uses a hand-rolled config rather than setupRestNscp so the two roles can
 * each have a user.
 */
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(900_000);

// sha256 of "{}" — the canonical serialisation of the empty facts document.
const EMPTY_DOCUMENT_HASH =
  "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";

describe("REST facts", () => {
  let nscp: NscpInstance;
  let adminKey: string | undefined = undefined;
  let monitoringKey: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      "/settings/WEB/server/users/watcher": {
        role: "monitoring",
        password: "watcher-password",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("can login", async () => {
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        adminKey = response.body.key;
        expect(adminKey).toBeDefined();
      });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("watcher", "watcher-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        monitoringKey = response.body.key;
        expect(monitoringKey).toBeDefined();
      });
  });

  it("requires authentication", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .trustLocalhost(true)
      .expect(403);
  });

  it("is listed in the v2 endpoint index", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts_url).toMatch(/\/api\/v2\/facts$/);
      });
  });

  it("serves an empty document when no fact set is enabled", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts).toEqual({});
        expect(response.body.enabled).toEqual([]);
        expect(response.body.errors).toEqual({});
        expect(response.body.revision).toEqual(0);
        // The empty document still has a hash: that is what lets a fleet
        // server tell "inventory switched off" from "agent too old to have
        // any".
        expect(response.body.hash).toEqual(EMPTY_DOCUMENT_HASH);
      });
  });

  it("tags the response with the document hash so a poller can revalidate", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.headers.etag).toEqual(`"${EMPTY_DOCUMENT_HASH}"`);
      });
  });

  it("answers 404 for a path nothing produced", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/os")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(404);
  });

  it("rejects a path that is not a dotted fact path", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/Not_A_Path")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(400);
  });

  it("lets the monitoring role read but not refresh", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${monitoringKey}`)
      .trustLocalhost(true)
      .expect(200);
    await request(REST_URL)
      .post("/api/v2/facts/refresh")
      .set("Authorization", `Bearer ${monitoringKey}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("refreshes on demand and returns the same envelope", async () => {
    await request(REST_URL)
      .post("/api/v2/facts/refresh")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts).toEqual({});
        expect(response.body.enabled).toEqual([]);
        // A round that collects nothing must not move the revision: a poller
        // watching it would otherwise see a change on every refresh.
        expect(response.body.revision).toEqual(0);
        expect(response.body.hash).toEqual(EMPTY_DOCUMENT_HASH);
      });
  });
});
