import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("query (legacy)", () => {
  let key = undefined;
  it("can login", async () => {
    await request(URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.user).toEqual("admin");
        expect(response.body.key).toBeDefined();
        key = response.body.key;
      });
  });

  it("can execute query (json)", async () => {
    await request(URL)
      .get("/query/mock_query?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "mock_query",
              lines: [
                {
                  message: "mock_query::a=b,c=d,e=f",
                  perf: [
                    {
                      alias: "a label",
                      float_value: {
                        critical: 30,
                        maximum: 50,
                        minimum: 10,
                        unit: "Z",
                        value: 30,
                        warning: 20,
                      },
                    },
                    {
                      alias: "another label",
                      float_value: {
                        critical: 30,
                        unit: "Z",
                        value: 33,
                        warning: 20,
                      },
                    },
                  ],
                },
              ],
              result: "OK",
            },
          ],
        });
      });
  });

  it("can execute query (json, warning)", async () => {
    await request(URL)
      .get("/query/check_warning?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_warning",
              lines: [
                {
                  message: "this is a message",
                  perf: [],
                },
              ],
              result: "WARNING",
            },
          ],
        });
      });
  });

  it("can execute query (json, critical)", async () => {
    await request(URL)
      .get("/query/check_critical?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_critical",
              lines: [
                {
                  message: "this is a message",
                  perf: [],
                },
              ],
              result: "CRITICAL",
            },
          ],
        });
      });
  });

  it("can execute query (json, unknown)", async () => {
    await request(URL)
      .get("/query/check_unknown?message=this+is+a+message")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          payload: [
            {
              command: "check_unknown",
              lines: [
                {
                  message: "Unknown command(s): check_unknown",
                  perf: [],
                },
              ],
              result: "UNKNOWN",
            },
          ],
        });
      });
  });
});
