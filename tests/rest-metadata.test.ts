/**
 * REST metadata scenarios — migrated from tests/rest/metadata.test.ts.
 *
 * Covers the metadata_controller endpoints:
 *   GET /api/v2/metadata           — index of available metadata resources
 *   GET /api/v2/metadata/counters  — PDH counter catalog (Windows + CheckSystem)
 *   GET /api/v2/metadata/channels  — registered submission channels
 *
 * CheckSystem is disabled in the shared fixture so /counters is expected to
 * return 500 with a "is the CheckSystem module loaded?" message. The error
 * shape is pinned here so a future regression that silently changes it
 * would surface.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST metadata controller", () => {
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
        expect(response.body.user).toEqual("admin");
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("GET /api/v2/metadata", () => {
    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/metadata").trustLocalhost(true).expect(403);
    });

    it("returns the index of metadata resources", async () => {
      await request(REST_URL)
        .get("/api/v2/metadata")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(Array.isArray(response.body)).toBe(true);
          const names = response.body.map((e: { name: string }) => e.name);
          expect(names).toEqual(expect.arrayContaining(["counters", "channels"]));
          for (const entry of response.body) {
            expect(typeof entry.url).toBe("string");
            expect(entry.url).toMatch(/\/api\/v[12]\/metadata\//);
            expect(typeof entry.title).toBe("string");
          }
        });
    });
  });

  describe("GET /api/v2/metadata/channels", () => {
    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/metadata/channels").trustLocalhost(true).expect(403);
    });

    it("returns an array of channel descriptors", async () => {
      await request(REST_URL)
        .get("/api/v2/metadata/channels")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(Array.isArray(response.body)).toBe(true);
          for (const channel of response.body) {
            expect(typeof channel.name).toBe("string");
            expect(Array.isArray(channel.plugins)).toBe(true);
          }
        });
    });
  });

  describe("GET /api/v2/metadata/counters", () => {
    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/metadata/counters").trustLocalhost(true).expect(403);
    });

    it("returns a server error when CheckSystem is not loaded", async () => {
      // The shared fixture disables CheckSystem. The controller forwards to
      // a CheckSystem command (pdh --list) and turns the empty response
      // into a 500 with a helpful message.
      await request(REST_URL)
        .get("/api/v2/metadata/counters")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(500)
        .then((response) => {
          expect(response.text).toMatch(/CheckSystem/);
        });
    });
  });

  describe("v1 alias still routed", () => {
    it("GET /api/v1/metadata returns the same shape", async () => {
      await request(REST_URL)
        .get("/api/v1/metadata")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(Array.isArray(response.body)).toBe(true);
        });
    });
  });
});
