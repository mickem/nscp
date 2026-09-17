/**
 * Exercises the "experimental" marking end-to-end: a module or check command
 * declares `"experimental": true` in its module.json, the build turns that
 * into a registry registration flag (commands) and the NSGetModuleFlags export
 * (modules), and the agent reports it on every listing it serves.
 *
 * What is asserted here is the REST surface, which is what the web UI reads;
 * the `nscp test` console markers are covered in command-client-console.test.ts.
 *
 * The commands used are picked for stable properties rather than behaviour:
 * `check_drivesize` is a long-settled CheckDisk command and `check_single_file`
 * is one of the ones added recently, so one must come back unmarked and the
 * other marked. If a command is later declared stable, move the assertion to
 * whichever command is experimental then - the point is that both answers are
 * reported, not that these two particular checks stay as they are.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(300_000);

interface Listed {
  name: string;
  experimental?: boolean;
}

describe("experimental marking", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckDisk");
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

  it("reports the flag on every query, marking only the experimental ones", async () => {
    const queries: Listed[] = await get("/api/v2/queries");
    const byName = (name: string) => queries.find((q) => q.name === name);

    // Every entry carries the field: a consumer can read it without having to
    // treat "missing" as a third answer.
    expect(queries.length).toBeGreaterThan(0);
    for (const query of queries) {
      expect(typeof query.experimental).toBe("boolean");
    }
    expect(byName("check_drivesize")?.experimental).toBe(false);
    expect(byName("check_single_file")?.experimental).toBe(true);
  });

  it("reports the flag when a single query is fetched", async () => {
    expect((await get("/api/v2/queries/check_single_file")).experimental).toBe(true);
    expect((await get("/api/v2/queries/check_drivesize")).experimental).toBe(false);
  });

  it("reports the flag on a loaded module", async () => {
    const modules: Listed[] = await get("/api/v2/modules");
    for (const module of modules) {
      expect(typeof module.experimental).toBe("boolean");
    }
    // CheckDisk itself is a settled module; only some of its commands are new.
    expect(modules.find((m) => m.name === "CheckDisk")?.experimental).toBe(false);
  });

  it("reports the flag for a module that is not loaded", async () => {
    // CheckSecurity is declared experimental and is not enabled here, so the
    // answer has to come from the on-disk inventory - the core reads the
    // module's flags export without starting it.
    expect((await get("/api/v2/modules/CheckSecurity")).experimental).toBe(true);
    expect((await get("/api/v2/modules/CheckHelpers")).experimental).toBe(false);
  });
});
