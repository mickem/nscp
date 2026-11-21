/**
 * REST on-demand fact sets, and the `modules` set.
 *
 * rest-facts.test.ts next door covers the stored document: sets an operator
 * enables, collected on a round. This suite covers the other kind - a set a
 * module collects when its path is read (`scripts.external`, `pdh.objects`)
 * and that is never part of the document - and the grants that go with it:
 *
 *   facts.get          the stored document, as before; no on-demand set
 *   facts.get.<set>    one set, stored or on demand, and nothing else
 *
 * It also holds the two older endpoints that now read these sets instead of
 * running a module's command line (`/api/v2/scripts/<runtime>`,
 * `/api/v2/metadata/counters`) to the answer they always gave.
 */
import request from "supertest";
import { NscpInstance, REST_URL, hasModule, onWindows } from "@fixtures/index";

jest.setTimeout(900_000);

const hasPython = hasModule("PythonScript");
const hasLua = hasModule("LUAScript");
const hasCheckSystem = hasModule("CheckSystem");

describe("REST on-demand facts", () => {
  let nscp: NscpInstance;
  const keys: Record<string, string> = {};

  async function login(user: string): Promise<void> {
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .auth(user, "default-password")
      .trustLocalhost(true)
      .expect(200);
    keys[user] = response.body.key;
  }

  function get(user: string, url: string) {
    return request(REST_URL)
      .get(url)
      .set("Authorization", `Bearer ${keys[user]}`)
      .trustLocalhost(true);
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckExternalScripts: "enabled",
        ...(hasPython ? { PythonScript: "enabled" } : {}),
        // LUAScript is deliberately not enabled: it is loaded at runtime
        // below, which is the case the core has to probe for.
        ...(onWindows && hasCheckSystem ? { CheckSystem: "enabled" } : {}),
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/external scripts/scripts": {
        on_demand_probe: "scripts/does-not-need-to-exist.sh",
      },
      "/settings/WEB/server/roles": {
        full: "*",
        // The document's grant and nothing finer: what every role that could
        // read facts before this held.
        inventory: "login.get,facts.get",
        // One on-demand set, and not the document.
        scripts_reader: "login.get,facts.get.scripts",
        // One stored set, and not the document.
        modules_reader: "login.get,facts.get.modules",
      },
      "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
      "/settings/WEB/server/users/inventory": { role: "inventory", password: "default-password" },
      "/settings/WEB/server/users/scripts": {
        role: "scripts_reader",
        password: "default-password",
      },
      "/settings/WEB/server/users/modules": {
        role: "modules_reader",
        password: "default-password",
      },
      "/settings/facts": {
        modules: "true",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    for (const user of ["admin", "inventory", "scripts", "modules"]) await login(user);
    // The startup round runs after the web server is up; wait for it to have
    // claimed the one stored set.
    const deadline = Date.now() + 60_000;
    for (;;) {
      const response = await get("admin", "/api/v2/facts").expect(200);
      if (response.body.enabled.includes("modules") || Date.now() > deadline) break;
      await new Promise((resolve) => setTimeout(resolve, 250));
    }
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  describe("the stored document", () => {
    it("is what it was: no on-demand set in it, none in `enabled`", async () => {
      const response = await get("admin", "/api/v2/facts").expect(200);
      expect(response.body.enabled).toEqual(["modules"]);
      expect(Object.keys(response.body.facts)).toEqual(["modules"]);
      // The envelope has the keys it always had and no others.
      expect(Object.keys(response.body).sort()).toEqual(
        ["collected", "enabled", "errors", "facts", "found", "gathered", "path", "revision"].sort(),
      );
    });

    it("carries the `modules` set once it is enabled", async () => {
      const response = await get("admin", "/api/v2/facts?path=modules.loaded").expect(200);
      expect(response.body.found).toBe(true);
      expect(response.body.on_demand).toBeUndefined();
      const loaded = response.body.facts as Array<Record<string, unknown>>;
      const ids = loaded.map((m) => m.id);
      // Sorted by id, each once.
      expect(ids).toEqual([...ids].sort());
      expect(new Set(ids).size).toBe(ids.length);
      const web = loaded.find((m) => m.name === "WEBServer");
      expect(web).toBeDefined();
      expect(web!.enabled).toBe(true);
      expect(typeof web!.version).toBe("string");
      expect(typeof web!.title).toBe("string");
      expect(loaded.find((m) => m.name === "CheckExternalScripts")).toBeDefined();
    });

    it("does not move its revision when an on-demand set is read", async () => {
      const before = await get("admin", "/api/v2/facts").expect(200);
      await get("admin", "/api/v2/facts?path=scripts.external").expect(200);
      const after = await get("admin", "/api/v2/facts").expect(200);
      expect(after.body.revision).toBe(before.body.revision);
      expect(after.body.facts).toEqual(before.body.facts);
    });
  });

  describe("GET /api/v2/facts/sets", () => {
    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/facts/sets").trustLocalhost(true).expect(403);
    });

    it("lists the stored sets and the on-demand ones, with the grant each takes", async () => {
      const response = await get("admin", "/api/v2/facts/sets").expect(200);
      expect(response.body.scheduled).toEqual(["modules"]);
      const external = response.body.on_demand.find(
        (s: Record<string, unknown>) => s.id === "scripts.external",
      );
      expect(external).toEqual({
        id: "scripts.external",
        parameterized: false,
        privilege: "facts.get.scripts",
      });
      if (hasPython) {
        expect(response.body.on_demand.map((s: Record<string, unknown>) => s.id)).toContain(
          "scripts.python",
        );
      }
    });

    it("is readable with the document's grant, and not with a per-set one", async () => {
      await get("inventory", "/api/v2/facts/sets").expect(200);
      await get("scripts", "/api/v2/facts/sets").expect(403);
    });
  });

  describe("scripts", () => {
    it("lists the external scripts folder and the registered commands", async () => {
      const response = await get("admin", "/api/v2/facts?path=scripts.external").expect(200);
      expect(response.body.found).toBe(true);
      expect(response.body.on_demand).toBe(true);
      expect(response.body.path).toBe("scripts.external");
      expect(Array.isArray(response.body.facts.files)).toBe(true);
      expect(response.body.facts.files.length).toBeGreaterThan(0);
      for (const file of response.body.facts.files) expect(typeof file).toBe("string");
      // The names scripts are registered under - never what they run.
      expect(response.body.facts.commands).toContain("on_demand_probe");
      expect(JSON.stringify(response.body.facts)).not.toContain("does-not-need-to-exist");
      // The listing only: two keys, and no way to ask for a script's source.
      expect(Object.keys(response.body.facts).sort()).toEqual(["commands", "files"]);
      // Read off the machine just now.
      expect(response.body.gathered["scripts.external"]).toMatch(
        /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/,
      );
      expect(response.body.errors["scripts.external"]).toBeUndefined();
    });

    it("answers a path below the set", async () => {
      const whole = await get("admin", "/api/v2/facts?path=scripts.external").expect(200);
      const files = await get("admin", "/api/v2/facts?path=scripts.external.files").expect(200);
      expect(files.body.found).toBe(true);
      expect(files.body.facts).toEqual(whole.body.facts.files);
      const missing = await get("admin", "/api/v2/facts?path=scripts.external.source").expect(200);
      expect(missing.body.found).toBe(false);
      expect(missing.body.on_demand).toBe(true);
    });

    it("answers the set itself with every loaded runtime", async () => {
      const response = await get("admin", "/api/v2/facts?path=scripts").expect(200);
      expect(response.body.found).toBe(true);
      expect(response.body.on_demand).toBe(true);
      expect(Object.keys(response.body.facts)).toContain("external");
      if (hasPython) {
        expect(Array.isArray(response.body.facts.python.files)).toBe(true);
        expect(response.body.facts.python.commands).toBeUndefined();
      }
    });

    it("says a runtime that is not loaded is not found, and still on demand", async () => {
      const response = await get("admin", "/api/v2/facts?path=scripts.nosuchruntime").expect(200);
      expect(response.body.found).toBe(false);
      expect(response.body.on_demand).toBe(true);
    });

    it("gives the scripts API the listing it always answered with", async () => {
      // /api/v2/scripts/ext used to run `list --json` through exec; it reads
      // the fact set now and has to answer with the same array.
      const facts = await get("admin", "/api/v2/facts?path=scripts.external").expect(200);
      const all = await get("admin", "/api/v2/scripts/ext?all=true").expect(200);
      expect(all.body).toEqual(facts.body.facts.files);
      const commands = await get("admin", "/api/v2/scripts/ext").expect(200);
      expect(commands.body).toEqual(facts.body.facts.commands);
      if (hasPython) {
        const python = await get("admin", "/api/v2/facts?path=scripts.python.files").expect(200);
        const listed = await get("admin", "/api/v2/scripts/py?all=true").expect(200);
        expect(listed.body).toEqual(python.body.facts);
        // "The queries" of the Python runtime is the registry's list.
        const queries = await get("admin", "/api/v2/scripts/py").expect(200);
        expect(Array.isArray(queries.body)).toBe(true);
        expect(queries.body).toContain("on_demand_probe");
      }
    });

    (hasLua ? it : it.skip)("serves a module loaded after the last round", async () => {
      // Not loaded yet: the scripts API says so the way it always has.
      await get("admin", "/api/v2/scripts/lua?all=true")
        .expect(500)
        .then((response) => expect(response.text).toMatch(/is the module loaded/));
      const before = await get("admin", "/api/v2/facts?path=scripts.lua").expect(200);
      expect(before.body.found).toBe(false);

      await get("admin", "/api/v2/modules/LUAScript/commands/load").expect(200);
      // No round has run since: the core asks the new module what it declares.
      const after = await get("admin", "/api/v2/facts?path=scripts.lua").expect(200);
      expect(after.body.found).toBe(true);
      expect(Array.isArray(after.body.facts.files)).toBe(true);
      const listed = await get("admin", "/api/v2/scripts/lua?all=true").expect(200);
      expect(listed.body).toEqual(after.body.facts.files);

      await get("admin", "/api/v2/modules/LUAScript/commands/unload").expect(200);
      const gone = await get("admin", "/api/v2/facts?path=scripts.lua").expect(200);
      expect(gone.body.found).toBe(false);
    });
  });

  describe("grants", () => {
    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/facts?path=scripts").trustLocalhost(true).expect(403);
    });

    it("does not let the document's grant read an on-demand set", async () => {
      // The role reads everything it could read before...
      const document = await get("inventory", "/api/v2/facts").expect(200);
      expect(Object.keys(document.body.facts)).toEqual(["modules"]);
      await get("inventory", "/api/v2/facts?path=modules").expect(200);
      // ...and is refused the listing, by name.
      for (const path of ["scripts", "scripts.external", "scripts.external.files"]) {
        await get("inventory", `/api/v2/facts?path=${path}`)
          .expect(403)
          .then((response) => expect(response.text).toContain("facts.get.scripts"));
      }
      // A path nobody produces is still just not found.
      const missing = await get("inventory", "/api/v2/facts?path=nosuchset").expect(200);
      expect(missing.body.found).toBe(false);
    });

    it("lets a per-set grant read that set and nothing else", async () => {
      const response = await get("scripts", "/api/v2/facts?path=scripts.external").expect(200);
      expect(response.body.found).toBe(true);
      await get("scripts", "/api/v2/facts").expect(403);
      await get("scripts", "/api/v2/facts?path=modules").expect(403);
      await get("scripts", "/api/v2/facts?path=pdh.objects").expect(403);
    });

    it("lets a per-set grant read one stored set without the rest of the document", async () => {
      const response = await get("modules", "/api/v2/facts?path=modules").expect(200);
      expect(response.body.found).toBe(true);
      expect(response.body.on_demand).toBeUndefined();
      await get("modules", "/api/v2/facts").expect(403);
      await get("modules", "/api/v2/facts?path=scripts").expect(403);
    });

    it("does not take a grant out of the path", async () => {
      // The first segment becomes the last segment of the grant; one that
      // could not name a set falls back to the document's grant.
      for (const path of ["*", "*.files", "scripts,modules", "Scripts"]) {
        await get("scripts", `/api/v2/facts?path=${encodeURIComponent(path)}`).expect(403);
        const response = await get("inventory", `/api/v2/facts?path=${encodeURIComponent(path)}`).expect(200);
        expect(response.body.found).toBe(false);
      }
    });

    it("does not let the script listing grant refresh the inventory", async () => {
      await request(REST_URL)
        .post("/api/v2/facts/commands/refresh")
        .set("Authorization", `Bearer ${keys.scripts}`)
        .trustLocalhost(true)
        .expect(403);
    });
  });

  describe("performance counters", () => {
    it("serves the catalog where PDH exists, and nothing where it does not", async () => {
      const response = await get("admin", "/api/v2/facts?path=pdh.objects").expect(200);
      if (!(onWindows && hasCheckSystem)) {
        // No producer declares it: an ordinary path nothing produced.
        expect(response.body.found).toBe(false);
        expect(response.body.on_demand).toBeUndefined();
        await get("admin", "/api/v2/metadata/counters")
          .expect(500)
          .then((r) => expect(r.text).toMatch(/CheckSystem/));
        return;
      }
      expect(response.body.found).toBe(true);
      expect(response.body.on_demand).toBe(true);
      const objects = response.body.facts as Array<{ id: string; counters: string[] }>;
      expect(objects.length).toBeGreaterThan(10);
      const ids = objects.map((o) => o.id);
      expect(new Set(ids).size).toBe(ids.length);
      // Language-neutral: every object is a named record with a string list.
      for (const object of objects) {
        expect(typeof object.id).toBe("string");
        expect(Array.isArray(object.counters)).toBe(true);
      }

      // The metadata endpoint is the same catalog, flattened to the paths it
      // has always returned, in the same order.
      const metadata = await get("admin", "/api/v2/metadata/counters").expect(200);
      const flattened = objects.flatMap((o) => o.counters.map((c) => `\\${o.id}\\${c}`));
      expect(metadata.body).toEqual(flattened);

      // The instances of one object, by a name out of the catalog.
      const withCounters = objects.find((o) => o.counters.length > 0)!;
      const instances = await get(
        "admin",
        `/api/v2/facts?path=${encodeURIComponent(`pdh.instances.${withCounters.id}`)}`,
      ).expect(200);
      expect(instances.body.found).toBe(true);
      expect(instances.body.facts.object).toBe(withCounters.id);
      expect(Array.isArray(instances.body.facts.names)).toBe(true);

      // A parameter is only ever matched against the catalog.
      for (const parameter of ["No Such Object 4711", "*", "\\Processor(*)\\*", "Memory --all"]) {
        const refused = await get(
          "admin",
          `/api/v2/facts?path=${encodeURIComponent(`pdh.instances.${parameter}`)}`,
        ).expect(200);
        expect(refused.body.found).toBe(false);
        expect(refused.body.errors["pdh.instances"]).toMatch(/No such performance object/);
      }
      // And one is required.
      const bare = await get("admin", "/api/v2/facts?path=pdh.instances").expect(200);
      expect(bare.body.found).toBe(false);
      expect(bare.body.errors["pdh.instances"]).toMatch(/takes a parameter/);

      // The set itself is the catalog; the parameterized key is left out.
      const whole = await get("admin", "/api/v2/facts?path=pdh").expect(200);
      expect(Object.keys(whole.body.facts)).toEqual(["objects"]);

      await get("inventory", "/api/v2/facts?path=pdh.objects")
        .expect(403)
        .then((r) => expect(r.text).toContain("facts.get.pdh"));
    });
  });
});
