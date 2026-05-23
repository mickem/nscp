/**
 * REST modules v1 scenarios — migrated from tests/rest/modules-v1.test.ts.
 *
 * Walks the full module lifecycle via /api/v1/modules: list, get,
 * load, get-after-load, enable, get-after-enable, disable, unload.
 * Targets CheckNSCP because it's small, side-effect free, and
 * disabled by default in the shared fixture.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

// Module name differs between platforms: "LuaScript" on Windows,
// "LUAScript" on Linux.
const luaScriptModule = expect.stringMatching(/^L[Uu][Aa]Script$/);
// plugin_id can be any number.
const anyPluginId = expect.stringMatching(/^\d+$/);

describe("REST modules (v1)", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("can login", async () => {
    await request(REST_URL)
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

  it("list", async () => {
    await request(REST_URL)
      .get("/api/v1/modules")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter(
            (module: { name: string }) =>
              module.name === "LuaScript" ||
              module.name === "LUAScript" ||
              module.name === "CheckNSCP",
          ),
        ).toEqual([
          {
            description: "Loads and processes internal Lua scripts",
            enabled: true,
            id: luaScriptModule,
            load_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/commands\/load$/,
            ),
            loaded: true,
            metadata: {
              alias: "",
              plugin_id: anyPluginId,
            },
            module_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/$/,
            ),
            name: luaScriptModule,
            title: "LUAScript",
            unload_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/commands\/unload$/,
            ),
          },
        ]);
      });
  });

  it("list (all)", async () => {
    await request(REST_URL)
      .get("/api/v1/modules?all=true")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter(
            (module: { name: string }) =>
              module.name === "LuaScript" ||
              module.name === "LUAScript" ||
              module.name === "CheckNSCP",
          ),
        ).toEqual([
          {
            description: "Loads and processes internal Lua scripts",
            enabled: true,
            id: luaScriptModule,
            load_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/commands\/load$/,
            ),
            loaded: true,
            metadata: {
              alias: "",
              plugin_id: anyPluginId,
            },
            module_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/$/,
            ),
            name: luaScriptModule,
            title: "LUAScript",
            unload_url: expect.stringMatching(
              /^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/commands\/unload$/,
            ),
          },
          {
            description: "Use this module to check the health and status of NSClient++ it self",
            enabled: false,
            id: "CheckNSCP",
            load_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/load",
            loaded: false,
            metadata: {
              alias: "",
              plugin_id: anyPluginId,
            },
            module_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/",
            name: "CheckNSCP",
            title: expect.anything(),
            unload_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/unload",
          },
        ]);
      });
  });

  it("get (unloaded)", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Use this module to check the health and status of NSClient++ it self",
          disable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/disable",
          enable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/enable",
          enabled: false,
          id: "CheckNSCP",
          load_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/load",
          loaded: false,
          metadata: {
            alias: "",
            plugin_id: anyPluginId,
          },
          name: "CheckNSCP",
          title: expect.anything(),
          unload_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/unload",
        });
      });
  });

  it("can load", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP/commands/load")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success load CheckNSCP");
      });
  });

  it("get (loaded)", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Use this module to check the health and status of NSClient++ it self",
          disable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/disable",
          enable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/enable",
          enabled: false,
          id: "CheckNSCP",
          load_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/load",
          loaded: true,
          metadata: {
            alias: "",
            plugin_id: anyPluginId,
          },
          name: "CheckNSCP",
          title: "CheckNSCP",
          unload_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/unload",
        });
      });
  });

  it("can enable", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP/commands/enable")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success enable CheckNSCP");
      });
  });

  it("get (enabled)", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Use this module to check the health and status of NSClient++ it self",
          disable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/disable",
          enable_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/enable",
          enabled: true,
          id: "CheckNSCP",
          load_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/load",
          loaded: true,
          metadata: {
            alias: "",
            plugin_id: anyPluginId,
          },
          name: "CheckNSCP",
          title: "CheckNSCP",
          unload_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/commands/unload",
        });
      });
  });

  it("can disable", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP/commands/disable")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success disable CheckNSCP");
      });
  });

  it("can unload", async () => {
    await request(REST_URL)
      .get("/api/v1/modules/CheckNSCP/commands/unload")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success unload CheckNSCP");
      });
  });
});
