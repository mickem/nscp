/**
 * The `runtime` segment of /api/v2/scripts.
 *
 * The URL's first segment used to be passed through verbatim whenever it was
 * not `ext` or `py`: the same string became both the permission suffix and the
 * module `exec_command()` was sent to. A role granted `scripts.*` rather than a
 * specific runtime could therefore drive the add/delete/show/list verbs of any
 * loaded module that implements them, and on PUT/DELETE the segment may be
 * empty, which asked the core to exec against "". It is an allow-list now, and
 * anything else is 400 before the permission check runs.
 *
 * `lua` is in the list for the first time: GET /api/v2/scripts has always
 * advertised it, but the mapping never translated it, so /api/v2/scripts/lua
 * addressed a module named "lua" which does not exist.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

describe("REST scripts runtime", () => {
  let nscp: NscpInstance;
  let key: string | undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await setupRestNscp(nscp);
    const response = await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200);
    key = response.body.key;
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it.each(["WEBServer", "CheckSystem", "CheckExternalScriptsX", "anything", "%2e%2e%2fCheckSystem"])(
    "answers 400 for the unknown runtime %s",
    async (runtime) => {
      const response = await request(REST_URL)
        .get(`/api/v2/scripts/${runtime}`)
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true);
      expect(response.status).toEqual(400);
    },
  );

  it("answers 400 for an empty runtime on DELETE", async () => {
    const response = await request(REST_URL)
      .delete("/api/v2/scripts//some-script")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    expect(response.status).toEqual(400);
  });

  it.each(["ext", "py", "lua"])("accepts the known runtime %s", async (runtime) => {
    // The module behind it may not be loaded in this instance, which is a 500
    // from the dispatch layer rather than the 400 this route now answers for a
    // name it does not know. Either way it got past the allow-list, which is
    // what this asserts.
    const response = await request(REST_URL)
      .get(`/api/v2/scripts/${runtime}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    expect(response.status).not.toEqual(400);
  });
});
