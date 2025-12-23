import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("info", () => {
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

  it("get_info", async () => {
    await request(URL)
      .get("/api/v2/info")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.version).toBeDefined();
          response.body.version = "N/A"; // Normalize version for test;

        expect(response.body).toEqual(
          {
              name: 'NSClient++',
              version: 'N/A',
              version_url: 'https://127.0.0.1:8443/api/v1/info/version'
          },
        );
      });
  });

  it("version", async () => {
    await request(URL)
      .get("/api/v2/info/version")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
          expect(response.body.version).toBeDefined();
          response.body.version = "N/A"; // Normalize version for test;
        expect(response.body).toEqual(
          {
            version: "N/A",
          });
      });
  });
});
