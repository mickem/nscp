/**
 * REST permissions scenarios — migrated from tests/rest/permissions.test.ts.
 *
 * Verifies the role-based access control configured under
 * [/settings/WEB/server/roles] (see setupRestNscp). Each user tier
 * (admin "full", legacy "legacy,login.get", client "public,...") is
 * exercised against /modules to confirm they get exactly the slice of
 * the API their role grants.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST permissions", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("admin user", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
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

    it("can access /modules", async () => {
      await request(REST_URL)
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
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.message).toEqual("Success load CheckLogFile");
          expect(response.body.result).toEqual(0);
        });
    });

    it("can unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body).toBeDefined();
          expect(response.body.message).toEqual("Success unload CheckLogFile");
          expect(response.body.result).toEqual(0);
        });
    });
  });

  describe("legacy user", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
      await request(REST_URL)
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
      await request(REST_URL)
        .get("/api/v2/modules")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not load a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });

  describe("client user", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
      await request(REST_URL)
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
      await request(REST_URL)
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
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/load")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not unload a module", async () => {
      await request(REST_URL)
        .get("/api/v2/modules/CheckLogFile/commands/unload")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can execute a query with arguments", async () => {
      // The counterpart to the restricted user below: `queries.execute`
      // still carries arguments, this change must not narrow it.
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute?a=b")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.lines[0].message).toEqual("mock_query::a=b");
        });
    });
  });

  // The `restricted` role holds `queries.execute.noargs` instead of
  // `queries.execute`: it may run the checks the agent defines but may not
  // shape what they do — the REST twin of the NRPE server's
  // `allow arguments = false`.
  describe("restricted user (no arguments)", () => {
    let key: string | undefined = undefined;
    it("can login", async () => {
      await request(REST_URL)
        .get("/api/v2/login")
        .auth("restricted", "restricted-password")
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.user).toEqual("restricted");
          expect(response.body.key).toBeDefined();
          key = response.body.key;
        });
    });

    it("can execute a query without arguments", async () => {
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.command).toEqual("mock_query");
          expect(response.body.lines[0].message).toEqual("mock_query::");
        });
    });

    it("can execute a query without arguments (nagios)", async () => {
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute_nagios")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.result).toEqual("OK");
        });
    });

    it("can not execute a query with arguments", async () => {
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute?a=b")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403)
        .then((response) => {
          expect(response.text).toContain("Arguments are not allowed");
        });
    });

    it("can not execute a query with a valueless argument", async () => {
      // `?show-all` carries no value but is still an argument: the check
      // sees it, so it must be refused like any other.
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute?show-all")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not execute a query with arguments (nagios)", async () => {
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/execute_nagios?a=b")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("gets 404 for an unknown command even with arguments", async () => {
      // The argument gate sits below the dispatch check: a command that does
      // not exist is a 404 for every caller, so a restricted client is not
      // told "arguments are not allowed" about an endpoint that was never
      // there.
      await request(REST_URL)
        .get("/api/v2/queries/mock_query/commands/no_such_command?a=b")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(404)
        .then((response) => {
          expect(response.text).toContain("unknown command");
        });
    });

    it("can run an alias that has arguments baked in", async () => {
      // Aliases are how an operator hands a no-arguments caller a check that
      // needs arguments: the alias resolves to `check_warning message=hello`
      // on the agent, the caller only names it.
      await request(REST_URL)
        .get("/api/v2/queries/echo_alias/commands/execute")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(200)
        .then((response) => {
          expect(response.body.lines[0].message).toEqual("hello");
        });
    });

    it("can not access /modules", async () => {
      await request(REST_URL)
        .get("/api/v2/modules")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });

    it("can not use the legacy query endpoint", async () => {
      await request(REST_URL)
        .get("/query/mock_query?a=b")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });
});
