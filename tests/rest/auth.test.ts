import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("GET /api/v2/login - no user", () => {
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
});

describe("GET /api/v2/login - admin", () => {
  it("responds with 404", async () => {
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
});

describe.skip("GET /api/v2/login - default-admin", () => {
  it("responds with 404", async () => {
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
});

describe("GET /api/v2/login - client", () => {
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
});

describe("GET /api/v2/login - legacy", () => {
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
