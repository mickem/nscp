import {beforeAll, describe, expect, it} from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("GET /api/v2/login - various users", () => {
  it("responds with 403", async () => {
    await request(URL)
      .get("/api/v2/login")
      .trustLocalhost(true)
      .expect(403)
      .then((response) => {
        expect(response.body.user).not.toBeDefined();
        expect(response.body.key).not.toBeDefined();
      });
  });

  it.skip("responds with 404", async () => {
    // TODO: Overriding admin password does not work correctly
    await request(URL)
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

  it.skip("responds with 404", async () => {
    // TODO: Default passwords does not work correctly
    await request(URL)
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

  it("responds with 404", async () => {
    await request(URL)
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

  it("responds with 404", async () => {
    await request(URL)
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
    let key = undefined;
    beforeAll(async () => {
        await request(URL)
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
        await request(URL)
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
        await request(URL)
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
        await request(URL)
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
        await request(URL)
            .get("/api/v2/info")
            .set("Authorization", `Bearer invalid-token`)
            .trustLocalhost(true)
            .expect(403)
            .then((response) => {
                expect(response.text).toEqual("403 You're not allowed");
            });
    });

    it("header-token", async () => {
        await request(URL)
            .get("/api/v2/info")
            .set("X-Auth-Token", key)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.version).toBeDefined();
            });
    });

    it("invalid header-token", async () => {
        await request(URL)
            .get("/api/v2/info")
            .set("X-Auth-Token", "invalid-token")
            .trustLocalhost(true)
            .expect(403)
            .then((response) => {
                expect(response.text).toEqual("403 You're not allowed");
            });

    });

    it("header-token-legacy", async () => {
        await request(URL)
            .get("/api/v2/info")
            .set("TOKEN", key)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.version).toBeDefined();
            });
    });

    it("invalid header-token-legacy", async () => {
        await request(URL)
            .get("/api/v2/info")
            .set("TOKEN", "invalid-token")
            .trustLocalhost(true)
            .expect(403)
            .then((response) => {
                expect(response.text).toEqual("403 You're not allowed");
            });
    });

    it("query-string-token", async () => {
        await request(URL)
            .get("/api/v2/info")
            .query({TOKEN: key})
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.version).toBeDefined();
            });
    });

    it("invalid query-string-token", async () => {
        await request(URL)
            .get("/api/v2/info")
            .query({TOKEN: "invalid-token"})
            .trustLocalhost(true)
            .expect(403)
            .then((response) => {
                expect(response.text).toEqual("403 You're not allowed");
            });
    });

    it("query-string-token-legacy", async () => {
        await request(URL)
            .get("/api/v2/info")
            .query({__TOKEN: key})
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.version).toBeDefined();
            });
    });

    it("invalid query-string-token-legacy", async () => {
        await request(URL)
            .get("/api/v2/info")
            .query({__TOKEN: "invalid-token"})
            .trustLocalhost(true)
            .expect(403)
            .then((response) => {
                expect(response.text).toEqual("403 You're not allowed");
            });
    });
});
