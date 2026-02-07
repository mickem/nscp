import {describe, expect, it} from "@jest/globals";
import request = require("supertest");

const URL = "https://127.0.0.1:8443";
// Module name differs between platforms: "LuaScript" on Windows, "LUAScript" on Linux
const luaScriptModule = expect.stringMatching(/^L[Uu][Aa]Script$/);
// plugin_id can be any number
const anyPluginId = expect.stringMatching(/^\d+$/);
describe("modules", () => {
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

    it("list", async () => {
        await request(URL)
            .get("/api/v2/modules")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.length).toBeGreaterThan(0);
                expect(
                    response.body.filter(
                        (module) =>
                            module.name === "LuaScript" || module.name === "LUAScript" || module.name === "CheckSystem",
                    ),
                ).toEqual([
                    {
                        description: "Loads and processes internal Lua scripts",
                        enabled: true,
                        id: luaScriptModule,
                        load_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v2\/modules\/L[Uu][Aa]Script\/commands\/load$/),
                        loaded: true,
                        metadata: {
                            alias: "",
                            plugin_id: anyPluginId,
                        },
                        module_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/$/),
                        name: luaScriptModule,
                        title: "LUAScript",
                        unload_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v2\/modules\/L[Uu][Aa]Script\/commands\/unload$/),
                    },
                ]);
            });
    });

    it("list (all)", async () => {
        await request(URL)
            .get("/api/v2/modules?all=true")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body.length).toBeGreaterThan(0);
                expect(
                    response.body.filter(
                        (module) =>
                            module.name === "LuaScript" || module.name === "LUAScript" || module.name === "CheckNSCP",
                    ),
                ).toEqual([
                    {
                        description: "Loads and processes internal Lua scripts",
                        enabled: true,
                        id: luaScriptModule,
                        load_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v2\/modules\/L[Uu][Aa]Script\/commands\/load$/),
                        loaded: true,
                        metadata: {
                            alias: "",
                            plugin_id: anyPluginId,
                        },
                        module_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v1\/modules\/L[Uu][Aa]Script\/$/),
                        name: luaScriptModule,
                        title: "LUAScript",
                        unload_url: expect.stringMatching(/^https:\/\/127\.0\.0\.1:8443\/api\/v2\/modules\/L[Uu][Aa]Script\/commands\/unload$/),
                    },
                    {
                        description: "Use this module to check the health and status of NSClient++ it self",
                        enabled: false,
                        id: "CheckNSCP",
                        load_url:
                            "https://127.0.0.1:8443/api/v2/modules/CheckNSCP/commands/load",
                        loaded: false,
                        metadata: {
                            alias: "",
                            plugin_id: anyPluginId,
                        },
                        module_url: "https://127.0.0.1:8443/api/v1/modules/CheckNSCP/",
                        name: "CheckNSCP",
                        title: expect.anything(),
                        unload_url:
                            "https://127.0.0.1:8443/api/v2/modules/CheckNSCP/commands/unload",
                    },
                ]);
            });
    });
    it("get (unloaded)", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    description: "Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA",
                    disable_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/disable",
                    enable_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/enable",
                    enabled: false,
                    id: "Scheduler",
                    load_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/load",
                    loaded: false,
                    metadata: {
                        alias: "",
                        plugin_id: anyPluginId,
                    },
                    name: "Scheduler",
                    title: expect.anything(),
                    unload_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/unload",
                });
            });
    });
    it("can load", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler/commands/load")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    message: "Success load Scheduler",
                    result: 0,
                });
            });
    });
    it("get (loaded)", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    description: "Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA",
                    disable_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/disable",
                    enable_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/enable",
                    enabled: false,
                    id: "Scheduler",
                    load_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/load",
                    loaded: true,
                    metadata: {
                        alias: "",
                        plugin_id: anyPluginId,
                    },
                    name: "Scheduler",
                    title: "Scheduler",
                    unload_url:
                        "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/unload",
                });
            });
    });

    it("can enable", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler/commands/enable")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    message: "Success enable Scheduler",
                    result: 0,
                });
            });
    });
    it("get (enabled)", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    description: "Use this to schedule check commands and jobs in conjunction with for instance passive monitoring through NSCA",
                    disable_url: "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/disable",
                    enable_url: "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/enable",
                    enabled: true,
                    id: "Scheduler",
                    load_url: "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/load",
                    loaded: true,
                    metadata: {
                        alias: "",
                        plugin_id: anyPluginId,
                    },
                    name: "Scheduler",
                    title: "Scheduler",
                    unload_url: "https://127.0.0.1:8443/api/v2/modules/Scheduler/commands/unload",
                });
            });
    });

    it("can disable", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler/commands/disable")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    message: "Success disable Scheduler",
                    result: 0,
                });
            });
    });

    it("can unload", async () => {
        await request(URL)
            .get("/api/v2/modules/Scheduler/commands/unload")
            .set("Authorization", `Bearer ${key}`)
            .trustLocalhost(true)
            .expect(200)
            .then((response) => {
                expect(response.body).toBeDefined();
                expect(response.body).toEqual({
                    message: "Success unload Scheduler",
                    result: 0,
                });
            });
    });
});
