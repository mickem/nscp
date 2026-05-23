/**
 * REST API auth scenarios — migrated from tests/rest/auth.test.ts.
 *
 * Covers the login endpoint with every user role, then enumerates the
 * supported auth schemes (basic, bearer, X-Auth-Token, TOKEN header,
 * query-string TOKEN/__TOKEN — the latter two should be rejected as of
 * 0.13).
 */
import request from "supertest";
import {
  NscpInstance,
  REST_URL,
  setupRestNscp,
} from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST auth", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("GET /api/v2/login - various users", () => {
    it("responds with 403 without auth", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.body.user).not.toBeDefined();
          expect(response.body.key).not.toBeDefined();
        });
    });

    it.skip("admin login returns a key", async () => {
      // TODO: Overriding admin password does not work correctly
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("admin", "default-password")
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", "application/json")
        .then((response) => {
          expect(response.body.user).toEqual("admin");
          expect(response.body.key).toBeDefined();
        });
    });

    it.skip("default-admin login returns a key", async () => {
      // TODO: Default passwords does not work correctly
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("default-admin", "default-password")
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", "application/json")
        .then((response) => {
          expect(response.body.user).toEqual("admin");
          expect(response.body.key).toBeDefined();
        });
    });

    it("client login returns a key", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("client", "client-password")
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", "application/json")
        .then((response) => {
          expect(response.body.user).toEqual("client");
          expect(response.body.key).toBeDefined();
        });
    });

    it("legacy login returns a key", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("legacy", "legacy-password")
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", "application/json")
        .then((response) => {
          expect(response.body.user).toEqual("legacy");
          expect(response.body.key).toBeDefined();
        });
    });
  });

  describe("Validate various auth schemes", () => {
    let key: string | undefined = undefined;
    beforeAll(async () => {
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

    it("basic-auth", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("admin", "default-password")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.user).toEqual("admin");
          expect(response.body.key).toBeDefined();
        });
    });

    it("invalid basic-auth", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("admin", "invalid-password")
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.body.user).not.toBeDefined();
          expect(response.body.key).not.toBeDefined();
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("bearer token", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.version).toBeDefined();
        });
    });

    it("invalid bearer token", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("Authorization", `Bearer invalid-token`)
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("header-token", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("X-Auth-Token", key!)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.version).toBeDefined();
        });
    });

    it("invalid header-token", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("X-Auth-Token", "invalid-token")
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("header-token-legacy", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("TOKEN", key!)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.version).toBeDefined();
        });
    });

    it("invalid header-token-legacy", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("TOKEN", "invalid-token")
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    // Tokens passed as URL query parameters (?TOKEN=... / ?__TOKEN=...) were
    // accepted by 0.12 as a fallback for clients that couldn't set headers.
    // 0.13 removed that path because URL parameters leak into browser
    // history, proxy logs and Referer headers; the server now returns 403
    // regardless of whether the supplied token is valid. The "valid" and
    // "invalid" cases below both assert this rejection so a future
    // accidental re-introduction would surface here.
    it("query-string-token rejected (security hardening, even with valid token)", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .query({ TOKEN: key })
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("invalid query-string-token", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .query({ TOKEN: "invalid-token" })
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("query-string-token-legacy rejected (security hardening, even with valid token)", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .query({ __TOKEN: key })
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });

    it("invalid query-string-token-legacy", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .query({ __TOKEN: "invalid-token" })
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toEqual("403 You're not allowed");
        });
    });
  });
});
