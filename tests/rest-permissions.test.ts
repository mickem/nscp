/**
 * REST permissions scenarios — migrated from tests/rest/permissions.test.ts.
 *
 * Verifies the role-based access control configured under
 * [/settings/WEB/server/roles] (see setupRestNscp). Each user tier
 * (admin "full", legacy "legacy,login.get", client "public,...") is
 * exercised against /modules to confirm they get exactly the slice of
 * the API their role grants.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST permissions", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("admin user", () => {
    let key: string | undefined = undefined;
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

    it("can access /modules", async () => {
      await request(REST_URL)
        .get("/api/v2/modules")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.length).toBeGreaterThan(0);
        });
    });

    it("can load a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.message).toEqual("Success load CheckLogFile");
          expect(response.body.result).toEqual(0);
        });
    });

    it("can unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.message).toEqual("Success unload CheckLogFile");
          expect(response.body.result).toEqual(0);
        });
    });
  });

  describe("legacy user", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("legacy", "legacy-password")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.user).toEqual("legacy");
          expect(response.body.key).toBeDefined();
          key = response.body.key;
        });
    });

    it("can not access /modules", async () => {
      await request(REST_URL)
        .get("/api/v2/modules")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not load a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });

  describe("client user", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("client", "client-password")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.user).toEqual("client");
          expect(response.body.key).toBeDefined();
          key = response.body.key;
        });
    });

    it("can access /modules", async () => {
      await request(REST_URL)
        .get("/api/v2/modules")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.length).toBeGreaterThan(0);
        });
    });

    it("can not load a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });
});
