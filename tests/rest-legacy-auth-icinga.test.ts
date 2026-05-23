/**
 * REST legacy-auth Icinga allowlist scenarios — migrated from
 * tests/rest/legacy-auth-icinga.test.ts.
 *
 * Legacy `?password=...` / `?TOKEN=...` query-string auth was removed in
 * commit 340b8db1 for security. The removal broke Icinga's bundled
 * check_nscp_api plugin, which still ships with the query-string mechanism.
 * To unblock that integration without re-exposing the vector to browsers
 * and arbitrary scrapers, the WEBServer gates the legacy path on a
 * User-Agent allowlist. Default: "Icinga/check_nscp_api". Configurable via
 * `[/settings/WEB/server] legacy query auth user agents`.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

const ICINGA_UA = "Icinga/check_nscp_api/2.14.0";

describe("REST legacy query-string auth — Icinga allowlist", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("With matching User-Agent (default allowlist)", () => {
    let icingaKey: string | undefined;

    it("GET /auth/token issues a token when ?password is correct", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", ICINGA_UA)
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", /application\/json/)
        .then((response) => {
          expect(response.body.status).toEqual("ok");
          expect(response.body["auth token"]).toBeDefined();
          icingaKey = response.body["auth token"];
        });
    });

    it("GET /auth/token rejects an incorrect password", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", ICINGA_UA)
        .query({ password: "wrong-password" })
        .trustLocalhost(true)
        .expect(403);
    });

    it("?TOKEN= query parameter authenticates the issued token", async () => {
      expect(icingaKey).toBeDefined();
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", ICINGA_UA)
        .query({ TOKEN: icingaKey })
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.version).toBeDefined();
        });
    });

    it("?__TOKEN= (legacy spelling) also authenticates", async () => {
      expect(icingaKey).toBeDefined();
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", ICINGA_UA)
        .query({ __TOKEN: icingaKey })
        .trustLocalhost(true)
        .expect(200);
    });

    it("GET /auth/logout revokes the token", async () => {
      expect(icingaKey).toBeDefined();
      await request(REST_URL)
        .get("/auth/logout")
        .set("User-Agent", ICINGA_UA)
        .query({ token: icingaKey })
        .trustLocalhost(true)
        .expect(200)
        .expect("Content-Type", /application\/json/)
        .then((response) => {
          expect(response.body.status).toEqual("ok");
        });
      // After logout the token must no longer authenticate, even from
      // the allowlisted User-Agent.
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", ICINGA_UA)
        .query({ TOKEN: icingaKey })
        .trustLocalhost(true)
        .expect(403);
    });

    it("matches case-insensitively", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", "icinga/check_nscp_api/dev")
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(200);
    });

    it("matches across version suffixes", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", "Icinga/check_nscp_api/3.0.0-rc1")
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(200);
    });
  });

  describe("Without matching User-Agent (default rejection)", () => {
    it("GET /auth/token returns 410 Gone for a browser-like UA", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set(
          "User-Agent",
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0",
        )
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(410)
        .expect("Content-Type", /application\/json/)
        .then((response) => {
          expect(response.body.status).toEqual("error");
          expect(response.body.message).toMatch(/removed|legacy|query parameter/i);
        });
    });

    it("GET /auth/token returns 410 Gone for curl", async () => {
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", "curl/8.0")
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(410);
    });

    it("GET /auth/logout returns 410 Gone for a browser-like UA", async () => {
      await request(REST_URL)
        .get("/auth/logout")
        .set("User-Agent", "Mozilla/5.0")
        .query({ token: "anything" })
        .trustLocalhost(true)
        .expect(410);
    });

    it("a UA mentioning Icinga but NOT 'Icinga/check_nscp_api' is rejected", async () => {
      // The default allowlist is anchored on the specific plugin name to
      // avoid admitting unrelated Icinga-flavoured tooling.
      await request(REST_URL)
        .get("/auth/token")
        .set("User-Agent", "MyIcingaProbe/1.0")
        .query({ password: "default-password" })
        .trustLocalhost(true)
        .expect(410);
    });
  });

  describe("Query-string token on non-auth endpoints", () => {
    let key: string | undefined;
    beforeAll(async () => {
      // Obtain a token through the modern path so we can test the legacy
      // query-string token vector independent of /auth/token.
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("admin", "default-password")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          key = response.body.key;
        });
    });

    it("accepts ?TOKEN= when User-Agent is allowlisted", async () => {
      expect(key).toBeDefined();
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", ICINGA_UA)
        .query({ TOKEN: key })
        .trustLocalhost(true)
        .expect(200);
    });

    it("rejects ?TOKEN= with a generic User-Agent (even with a valid token)", async () => {
      expect(key).toBeDefined();
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", "Mozilla/5.0")
        .query({ TOKEN: key })
        .trustLocalhost(true)
        .expect(403);
    });

    it("rejects ?TOKEN= when the token is invalid even from allowlisted UA", async () => {
      await request(REST_URL)
        .get("/api/v2/info")
        .set("User-Agent", ICINGA_UA)
        .query({ TOKEN: "this-is-not-a-real-token" })
        .trustLocalhost(true)
        .expect(403);
    });
  });
});
