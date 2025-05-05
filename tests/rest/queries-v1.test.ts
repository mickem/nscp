import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("queries", () => {
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

  it("list queries", async () => {
    await request(URL)
      .get("/api/v1/queries")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter((query) => query.name === "mock_query"),
        ).toEqual([
          {
            description: "Mock query used during tests",
            metadata: {},
            name: "mock_query",
            plugin: "LuaScript",
            query_url: "https://127.0.0.1:8443/api/v1/queries/mock_query/",
            title: "mock_query",
          },
        ]);
      });
  });

  it("list (all) queries", async () => {
    await request(URL)
      .get("/api/v1/queries?all=true")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter((query) => query.name === "mock_query"),
        ).toEqual([
          {
            description: "Mock query used during tests",
            metadata: {},
            name: "mock_query",
            plugin: "LuaScript",
            query_url: "https://127.0.0.1:8443/api/v1/queries/mock_query/",
            title: "mock_query",
          },
        ]);
      });
  });

  it("get query", async () => {
    await request(URL)
      .get("/api/v1/queries/mock_query")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Mock query used during tests",
          execute_nagios_url:
            "https://127.0.0.1:8443/api/v1/queries/mock_query/commands/execute_nagios",
          execute_url:
            "https://127.0.0.1:8443/api/v1/queries/mock_query/commands/execute",
          metadata: {},
          name: "mock_query",
          plugin: "LuaScript",
          title: "mock_query",
        });
      });
  });
  it("can execute query (json)", async () => {
    await request(URL)
      .get("/api/v1/queries/mock_query/commands/execute?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          command: "mock_query",
          lines: [
            {
              message: "mock_query::a=b,c=d,e=f",
              perf: {
                "a label": {
                  critical: 30,
                  maximum: 50,
                  minimum: 10,
                  unit: "Z",
                  value: 30,
                  warning: 20,
                },
                "another label": {
                  critical: 30,
                  unit: "Z",
                  value: 33,
                  warning: 20,
                },
              },
            },
          ],
          result: 0,
        });
      });
  });

  it("can execute query (text)", async () => {
    await request(URL)
      .get("/api/v1/queries/mock_query/commands/execute?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .set("Accept", "text/plain")
      .set("Content-Type", "text/plain")
      .trustLocalhost(true)
      .expect(200)
      .expect("Content-Type", "text/plain")
      .then((response) => {
        expect(response.text).toEqual(
          "mock_query::a=b,c=d,e=f|'a label'=30Z;20;30;10;50 'another label'=33Z;20;30\n",
        );
      });
  });

  it("can execute query (json)", async () => {
    await request(URL)
      .get("/api/v1/queries/mock_query/commands/execute_nagios?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          command: "mock_query",
          lines: [
            {
              message: "mock_query::a=b,c=d,e=f",
              perf: "'a label'=30Z;20;30;10;50 'another label'=33Z;20;30",
            },
          ],
          result: "OK",
        });
      });
  });

  it("can execute query (text)", async () => {
    await request(URL)
      .get("/api/v1/queries/mock_query/commands/execute_nagios?a=b&c=d&e=f")
      .set("Authorization", `Bearer ${key}`)
      .set("Accept", "text/plain")
      .set("Content-Type", "text/plain")
      .trustLocalhost(true)
      .expect(200)
      .expect("Content-Type", "text/plain")
      .then((response) => {
        expect(response.text).toEqual(
          "mock_query::a=b,c=d,e=f|'a label'=30Z;20;30;10;50 'another label'=33Z;20;30\n",
        );
      });
  });
});
