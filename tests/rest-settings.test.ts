/**
 * REST settings scenarios — migrated from tests/rest/settings.test.ts.
 *
 * Covers GET / PUT / DELETE on /api/v2/settings/... plus /status. The
 * DELETE method shipped in commit aad7c3f8 alongside the existing
 * GET / PUT shape; the round-trip test pins that lifecycle. Operates
 * under a dedicated /settings/rest-test/scratch path so it can't
 * perturb anything the rest of the suite relies on.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST settings controller", () => {
  let nscp: NscpInstance;
  let key: string | undefined;
  const TEST_PATH = "/settings/rest-test/scratch";

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

  describe("auth gating", () => {
    it("GET /api/v2/settings/... requires authentication", async () => {
      await request(REST_URL).get(`/api/v2/settings${TEST_PATH}`).trustLocalhost(true).expect(403);
    });

    it("PUT /api/v2/settings/... requires authentication", async () => {
      await request(REST_URL)
        .put(`/api/v2/settings${TEST_PATH}`)
        .send([{ key: "foo", value: "bar" }])
        .trustLocalhost(true)
        .expect(403);
    });

    it("DELETE /api/v2/settings/... requires authentication", async () => {
      await request(REST_URL)
        .delete(`/api/v2/settings${TEST_PATH}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });

  describe("GET /api/v2/settings/status", () => {
    it("returns a status object", async () => {
      await request(REST_URL)
        .get("/api/v2/settings/status")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
        });
    });
  });

  describe("PUT then GET round-trip", () => {
    it("writes a key via PUT", async () => {
      await request(REST_URL)
        .put(`/api/v2/settings${TEST_PATH}`)
        .set("Authorization", `Bearer ${key}`)
        .send([{ key: "marker", value: "hello-from-rest-test" }])
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.status).toEqual("success");
          expect(response.body.keys).toEqual(1);
        });
    });

    it("reads the key back via GET", async () => {
      await request(REST_URL)
        .get(`/api/v2/settings${TEST_PATH}`)
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(Array.isArray(response.body)).toBe(true);
          const node = response.body.find((n: { key: string }) => n.key === "marker");
          expect(node).toBeDefined();
          expect(node.value).toEqual("hello-from-rest-test");
        });
    });
  });

  describe("DELETE method (commit aad7c3f8)", () => {
    it("removes a key via DELETE with a body", async () => {
      // Ensure the key exists first so the delete has something to act on.
      await request(REST_URL)
        .put(`/api/v2/settings${TEST_PATH}`)
        .set("Authorization", `Bearer ${key}`)
        .send([{ key: "to-delete", value: "transient" }])
        .trustLocalhost(true)
        .expect(200);

      await request(REST_URL)
        .delete(`/api/v2/settings${TEST_PATH}`)
        .set("Authorization", `Bearer ${key}`)
        .set("Content-Type", "application/json")
        .send([{ key: "to-delete" }])
        .trustLocalhost(true)
        .expect(200);

      // Verify the key is gone.
      await request(REST_URL)
        .get(`/api/v2/settings${TEST_PATH}`)
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          const node = response.body.find((n: { key: string }) => n.key === "to-delete");
          expect(node).toBeUndefined();
        });
    });

    it("rejects DELETE with no key and no path (safety net)", async () => {
      // Path-level deletes need either a body (so the intent is explicit)
      // or no body at all — an empty entry in a batch must not silently
      // succeed.
      await request(REST_URL)
        .delete(`/api/v2/settings`)
        .set("Authorization", `Bearer ${key}`)
        .set("Content-Type", "application/json")
        .send([{}])
        .trustLocalhost(true)
        .expect(400);
    });
  });
});
