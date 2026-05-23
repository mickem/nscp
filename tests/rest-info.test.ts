/**
 * REST info scenarios — migrated from tests/rest/info.test.ts.
 *
 * Covers /api/v2/info and /api/v2/info/version. Version strings are
 * normalized to "N/A" before equality so the shape — not the build
 * number — is what's pinned.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST info", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
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
        expect(response.body.user).toEqual("admin");
        expect(response.body.key).toBeDefined();
        key = response.body.key;
      });
  });

  it("get_info", async () => {
    await request(REST_URL)
      .get("/api/v2/info")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.version).toBeDefined();
        response.body.version = "N/A"; // Normalize version for test;

        expect(response.body).toEqual({
          name: "NSClient++",
          version: "N/A",
          version_url: "https://127.0.0.1:8443/api/v1/info/version",
        });
      });
  });

  it("version", async () => {
    await request(REST_URL)
      .get("/api/v2/info/version")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.version).toBeDefined();
        response.body.version = "N/A"; // Normalize version for test;
        expect(response.body).toEqual({
          version: "N/A",
        });
      });
  });
});
