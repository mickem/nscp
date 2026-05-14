import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";

// Aliases are defined under [/settings/check helpers/alias] in nsclient.ini -
// the CheckHelpers module registers them as QUERY_ALIAS items at load time.
const expectedAliases = ["mock_alias", "echo_alias"];

describe("aliases (v2)", () => {
  let key: string | undefined = undefined;

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

  it("list aliases returns the registered QUERY_ALIAS items", async () => {
    await request(URL)
      .get("/api/v2/aliases")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(Array.isArray(response.body)).toBe(true);
        const names = response.body.map((a: { name: string }) => a.name);
        for (const expected of expectedAliases) {
          expect(names).toContain(expected);
        }
      });
  });

  it("alias entries expose name, query_url and metadata", async () => {
    await request(URL)
      .get("/api/v2/aliases")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        const alias = response.body.find(
          (a: { name: string }) => a.name === "mock_alias",
        );
        expect(alias).toBeDefined();
        // The list operation reports executions through the queries endpoint
        // because the core dispatches aliases just like any other query.
        expect(alias.query_url).toEqual(
          "https://127.0.0.1:8443/api/v2/queries/mock_alias/",
        );
        expect(alias.metadata).toBeDefined();
      });
  });

  it("does not include real (non-alias) queries", async () => {
    await request(URL)
      .get("/api/v2/aliases")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        const names = response.body.map((a: { name: string }) => a.name);
        // mock_query is registered as a plain QUERY (Registry:simple_function
        // in mock.lua), so it must not show up under /aliases.
        expect(names).not.toContain("mock_query");
      });
  });

  it("?all=false returns aliases from loaded modules only", async () => {
    await request(URL)
      .get("/api/v2/aliases?all=false")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(Array.isArray(response.body)).toBe(true);
      });
  });

  it("rejects callers without the aliases.list grant", async () => {
    // The legacy role only carries `legacy,login.get` so it must NOT be
    // able to read /aliases. This guards against accidentally widening
    // the default grant set.
    let legacyKey: string | undefined = undefined;
    await request(URL)
      .get("/api/v2/login")
      .auth("legacy", "legacy-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        legacyKey = response.body.key;
      });
    await request(URL)
      .get("/api/v2/aliases")
      .set("Authorization", `Bearer ${legacyKey}`)
      .trustLocalhost(true)
      .expect(403);
  });
});

describe("aliases (v1)", () => {
  let key: string | undefined = undefined;

  it("can login", async () => {
    await request(URL)
      .get("/api/v1/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  it("is mounted on /api/v1 as well", async () => {
    await request(URL)
      .get("/api/v1/aliases")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(Array.isArray(response.body)).toBe(true);
        const alias = response.body.find(
          (a: { name: string }) => a.name === "mock_alias",
        );
        expect(alias).toBeDefined();
        // v1 endpoint returns v1 query URLs.
        expect(alias.query_url).toEqual(
          "https://127.0.0.1:8443/api/v1/queries/mock_alias/",
        );
      });
  });
});
