/**
 * REST API discovery scenarios — migrated from tests/rest/api-discovery.test.ts.
 *
 * Covers the api_controller index endpoints:
 *   GET /api          — list available API versions
 *   GET /api/v1       — list endpoints in the legacy API
 *   GET /api/v2       — list endpoints in the current API
 * plus the only legacy_controller endpoint not covered elsewhere:
 *   GET /core/isalive — liveness probe, unauthenticated
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST api discovery", () => {
  let nscp: NscpInstance;
  let key: string | undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);

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

  it("GET /api requires authentication", async () => {
    await request(REST_URL).get("/api").trustLocalhost(true).expect(403);
  });

  it("GET /api returns the version index", async () => {
    await request(REST_URL)
      .get("/api")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.current_api).toEqual("https://127.0.0.1:8443/api/v2");
        expect(response.body.legacy_api).toEqual("https://127.0.0.1:8443/api/v1");
        expect(response.body.beta_api).toEqual("https://127.0.0.1:8443/api/v2");
      });
  });

  it("GET /api/v2 lists the endpoint URLs", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.scripts_url).toMatch(/\/api\/v2\/scripts$/);
        expect(response.body.modules_url).toMatch(/\/api\/v2\/modules$/);
        expect(response.body.queries_url).toMatch(/\/api\/v2\/queries$/);
        expect(response.body.aliases_url).toMatch(/\/api\/v2\/aliases$/);
        expect(response.body.settings_url).toMatch(/\/api\/v2\/settings$/);
        expect(response.body.logs_url).toMatch(/\/api\/v2\/logs$/);
        expect(response.body.info_url).toMatch(/\/api\/v2\/info$/);
      });
  });

  it("GET /api/v1 returns the same endpoint shape (alias)", async () => {
    await request(REST_URL)
      .get("/api/v1")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.modules_url).toBeDefined();
        expect(response.body.queries_url).toBeDefined();
        expect(response.body.info_url).toBeDefined();
      });
  });

  describe("legacy core endpoints", () => {
    it("GET /core/isalive returns a status object without authentication", async () => {
      // Liveness probe — intentionally open so monitoring stacks can call
      // it before any credentials are minted.
      await request(REST_URL)
        .get("/core/isalive")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.status).toBeDefined();
          expect(typeof response.body.status).toBe("string");
        });
    });
  });
});
