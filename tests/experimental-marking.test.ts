/**
 * Exercises the "experimental" marking end-to-end: a module or check command
 * declares `"experimental": true` in its module.json, the build turns that
 * into a registry registration flag (commands) and the NSGetModuleFlags export
 * (modules), and the agent reports it on every listing it serves.
 *
 * Every expectation here is read from the module.json manifests in the source
 * tree rather than written down again, because which commands carry the flag
 * is *meant* to change: a command keeps it until its options and output
 * settle, then loses it. What is being tested is that whatever the manifest
 * declares is what the API reports - in both directions - not that any
 * particular check is experimental today.
 *
 * The REST surface is what the web UI reads; the `nscp test` console markers
 * are covered in command-client-console.test.ts.
 */
import request from "supertest";
import { NscpInstance, REST_URL, moduleManifest, moduleManifests, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(300_000);

/** The module the agent is started with: present and loadable on both platforms. */
const LOADED_MODULE = "CheckDisk";

interface Listed {
  name: string;
  plugin?: string;
  experimental?: boolean;
}

describe("experimental marking", () => {
  let nscp: NscpInstance;
  let key: string;
  const manifests = moduleManifests();
  const loaded = moduleManifest(LOADED_MODULE);

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, LOADED_MODULE);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /** GET a JSON endpoint as the admin user. */
  async function get(path: string, expected = 200) {
    const response = await request(REST_URL)
      .get(path)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(expected);
    return response.body;
  }

  it("reports each query exactly as its module declares it", async () => {
    const queries: Listed[] = await get("/api/v2/queries");
    expect(queries.length).toBeGreaterThan(0);

    // Every entry carries the field, so a consumer never has to treat
    // "missing" as a third answer.
    for (const query of queries) {
      expect(typeof query.experimental).toBe("boolean");
    }

    // Compare against the manifest of whichever module owns each query. A
    // query whose name the manifest does not list (an alias, or a command a
    // script registered) has nothing to compare against and is skipped.
    const compared: string[] = [];
    for (const query of queries) {
      const manifest = query.plugin ? manifests.get(query.plugin) : undefined;
      const declared = manifest?.byLowerName.get(query.name.toLowerCase());
      if (declared === undefined) continue;
      compared.push(query.name);
      expect({ name: query.name, experimental: query.experimental }).toEqual({
        name: query.name,
        experimental: declared,
      });
    }
    expect(compared.length).toBeGreaterThan(0);
  });

  it("reports the flag when a single query is fetched", async () => {
    // Take the names from the listing rather than the manifest, so the request
    // uses exactly the spelling the registry answers to. One of each kind,
    // where the loaded module still has both.
    const queries: Listed[] = await get("/api/v2/queries");
    const mine = queries.filter(
      (q) => q.plugin === LOADED_MODULE && loaded.byLowerName.has(q.name.toLowerCase()),
    );
    const pick = (experimental: boolean) =>
      mine.find((q) => loaded.byLowerName.get(q.name.toLowerCase()) === experimental);

    let checked = 0;
    for (const experimental of [true, false]) {
      const query = pick(experimental);
      if (!query) continue;
      const fetched = await get(`/api/v2/queries/${query.name}`);
      expect({ name: query.name, experimental: fetched.experimental }).toEqual({
        name: query.name,
        experimental,
      });
      checked++;
    }
    expect(checked).toBeGreaterThan(0);
  });

  it("reports each module exactly as its manifest declares it", async () => {
    // ?all=true walks the module directory, so this covers modules that are
    // loaded and modules that are only on disk - the latter answered from the
    // flags export without the module ever being started.
    const modules: Listed[] = await get("/api/v2/modules?all=true");
    expect(modules.length).toBeGreaterThan(0);

    const compared: string[] = [];
    for (const module of modules) {
      expect(typeof module.experimental).toBe("boolean");
      const manifest = manifests.get(module.name);
      if (!manifest) continue;
      compared.push(module.name);
      expect({ name: module.name, experimental: module.experimental }).toEqual({
        name: module.name,
        experimental: manifest.experimental,
      });
    }
    // The loaded module is always among them, so this can only fail if the
    // listing stopped naming modules the way the manifests do.
    expect(compared).toContain(LOADED_MODULE);
  });

  it("reports the flag for a module fetched by name while it is not loaded", async () => {
    // Fetching one module by name is the path that inventories it on disk.
    // Any module that is built but not loaded here will do; which ones exist
    // differs per platform and build, so take them from the listing.
    const modules: Listed[] = await get("/api/v2/modules?all=true");
    const candidates = modules.filter((m) => m.name !== LOADED_MODULE && manifests.has(m.name));
    expect(candidates.length).toBeGreaterThan(0);

    // Prefer one of each kind, so the "true" answer is exercised whenever a
    // module on this machine declares it.
    const pick = (experimental: boolean) =>
      candidates.find((m) => manifests.get(m.name)!.experimental === experimental);
    for (const module of [pick(true), pick(false)]) {
      if (!module) continue;
      const declared = manifests.get(module.name)!.experimental;
      expect({ name: module.name, experimental: (await get(`/api/v2/modules/${module.name}`)).experimental }).toEqual(
        { name: module.name, experimental: declared },
      );
    }
  });
  it("makes a module's own flag cover the commands it registers", async () => {
    // A module that is experimental as a whole says so once, at the top of its
    // module.json, and its commands carry no flag of their own - so this is
    // the only place that inheritance is visible. Which module provides it is
    // read from the manifests: any one that declares itself experimental and
    // leaves its commands undeclared, and that this build actually has.
    const listed: Listed[] = await get("/api/v2/modules?all=true");
    const candidates = listed
      .map((m) => manifests.get(m.name))
      .filter((m): m is NonNullable<typeof m> => !!m && m.experimental && m.commands.size > 0);
    if (candidates.length === 0) return; // nothing declares itself experimental any more

    // Loading can fail for reasons of its own (a module needing configuration
    // this instance does not have), so try the candidates in turn.
    let loadedModule: (typeof candidates)[number] | undefined;
    for (const candidate of candidates) {
      const result = await get(`/api/v2/modules/${candidate.name}/commands/load`);
      if (result?.result === 0) {
        loadedModule = candidate;
        break;
      }
    }
    expect(loadedModule).toBeDefined();

    const queries: Listed[] = await get("/api/v2/queries");
    const mine = queries.filter((q) => q.plugin === loadedModule!.name);
    expect(mine.length).toBeGreaterThan(0);
    for (const query of mine) {
      expect({ name: query.name, experimental: query.experimental }).toEqual({
        name: query.name,
        experimental: true,
      });
    }
  });

});
