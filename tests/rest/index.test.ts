import { describe, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("GET /", () => {
  it("responds with 200", async () => {
    await request(URL)
      .get("/")
      .trustLocalhost(true)
      .expect("Content-Type", "text/html")
      .expect(200);
  });
});

describe("GET /invalid", () => {
  it("responds with 404", async () => {
    request(URL).get("/invalid").trustLocalhost(true).expect(404);
  });
});
