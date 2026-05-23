/**
 * REST static index scenarios — migrated from tests/rest/index.test.ts.
 *
 * Covers the StaticController fallback that serves the bundled SPA's
 * index.html for `/` and emits 404 for unknown paths.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST static index", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("GET /", () => {
    it("responds with 200", async () => {
      await request(REST_URL)
        .get("/")
        .trustLocalhost(true)
        .expect("Content-Type", "text/html")
        .expect(200);
    });
  });

  describe("GET /invalid", () => {
    it("responds with 404", async () => {
      request(REST_URL).get("/invalid").trustLocalhost(true).expect(404);
    });
  });
});
