import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("admin user", () => {
  let key = undefined;
  it("can login", async () => {
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

  it("can access /modules", async () => {
    await request(URL)
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
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/load")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.message).toEqual("Success load CheckDisk");
        expect(response.body.result).toEqual(0);
      });
  });

  it("can unload a module", async () => {
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/unload")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.message).toEqual("Success unload CheckDisk");
        expect(response.body.result).toEqual(0);
      });
  });
});

describe("legacy user", () => {
  let key = undefined;
  it("can login", async () => {
    await request(URL)
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
    await request(URL)
      .get("/api/v2/modules")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("can not load a module", async () => {
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/load")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("can not unload a module", async () => {
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/unload")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(403);
  });
});

describe("client user", () => {
  let key = undefined;
  it("can login", async () => {
    await request(URL)
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
    await request(URL)
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
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/load")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("can not unload a module", async () => {
    await request(URL)
      .get("/api/v2/modules/CheckDisk/commands/unload")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(403);
  });
});
