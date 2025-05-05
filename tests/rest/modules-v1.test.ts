import { describe, expect, it } from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
describe("modules", () => {
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

  it("list", async () => {
    await request(URL)
      .get("/api/v1/modules")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter(
            (module) =>
              module.name === "LuaScript" || module.name === "CheckSystem",
          ),
        ).toEqual([
          {
            description: "Loads and processes internal Lua scripts",
            enabled: true,
            id: "LuaScript",
            load_url:
              "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/load",
            loaded: true,
            metadata: {
              alias: "",
              plugin_id: "1",
            },
            module_url: "https://127.0.0.1:8443/api/v1/modules/LuaScript/",
            name: "LuaScript",
            title: "LUAScript",
            unload_url:
              "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/unload",
          },
        ]);
      });
  });

  it("list (all)", async () => {
    await request(URL)
      .get("/api/v1/modules?all=true")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body.length).toBeGreaterThan(0);
        expect(
          response.body.filter(
            (module) =>
              module.name === "LuaScript" || module.name === "CheckSystem",
          ),
        ).toEqual([
          {
            description: "Loads and processes internal Lua scripts",
            enabled: true,
            id: "LuaScript",
            load_url:
              "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/load",
            loaded: true,
            metadata: {
              alias: "",
              plugin_id: "1",
            },
            module_url: "https://127.0.0.1:8443/api/v1/modules/LuaScript/",
            name: "LuaScript",
            title: "LUAScript",
            unload_url:
              "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/unload",
          },
          {
            description:
              "Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.",
            enabled: false,
            id: "CheckSystem",
            load_url:
              "https://127.0.0.1:8443/api/v1/modules/CheckSystem/commands/load",
            loaded: false,
            metadata: {
              alias: "",
              plugin_id: "14",
            },
            module_url: "https://127.0.0.1:8443/api/v1/modules/CheckSystem/",
            name: "CheckSystem",
            title: "",
            unload_url:
              "https://127.0.0.1:8443/api/v1/modules/CheckSystem/commands/unload",
          },
        ]);
      });
  });

  it("get", async () => {
    await request(URL)
      .get("/api/v1/modules/LuaScript")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Loads and processes internal Lua scripts",
          disable_url:
            "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/disable",
          enable_url:
            "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/enable",
          enabled: true,
          id: "LuaScript",
          load_url:
            "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/load",
          loaded: true,
          metadata: {
            alias: "",
            plugin_id: "1",
          },
          name: "LuaScript",
          title: "LUAScript",
          unload_url:
            "https://127.0.0.1:8443/api/v1/modules/LuaScript/commands/unload",
        });
      });
  });
  it("get (unloaded)", async () => {
    await request(URL)
      .get("/api/v1/modules/CheckEventLog")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body).toBeDefined();
        expect(response.body).toEqual({
          description: "Check for errors and warnings in the event log.",
          disable_url:
            "https://127.0.0.1:8443/api/v1/modules/CheckEventLog/commands/disable",
          enable_url:
            "https://127.0.0.1:8443/api/v1/modules/CheckEventLog/commands/enable",
          enabled: false,
          id: "CheckEventLog",
          load_url:
            "https://127.0.0.1:8443/api/v1/modules/CheckEventLog/commands/load",
          loaded: false,
          metadata: {
            alias: "",
            plugin_id: "7",
          },
          name: "CheckEventLog",
          title: "",
          unload_url:
            "https://127.0.0.1:8443/api/v1/modules/CheckEventLog/commands/unload",
        });
      });
  });
  it("can load", async () => {
    await request(URL)
      .get("/api/v1/modules/CheckEventLog/commands/load")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success load CheckEventLog");
      });
  });

  it("can enable", async () => {
    await request(URL)
      .get("/api/v1/modules/CheckEventLog/commands/enable")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success enable CheckEventLog");
      });
  });

  it("can disable", async () => {
    await request(URL)
      .get("/api/v1/modules/CheckEventLog/commands/disable")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success disable CheckEventLog");
      });
  });

  it("can unload", async () => {
    await request(URL)
      .get("/api/v1/modules/CheckEventLog/commands/unload")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.text).toEqual("Success unload CheckEventLog");
      });
  });
});
