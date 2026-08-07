/**
 * REST host-tags scenarios: /api/v2/tags serves the central tag repository —
 * key=value facts contributed by modules through the tag API (NSAPISetTag).
 *
 * CheckDisk is enabled on top of the REST baseline so a real producer runs:
 * on Windows it publishes `drives=c:,d:,...` at load. On other platforms no
 * baseline module publishes tags, so the endpoint just returns an empty
 * object — asserted too, since "no tags" must be a valid, well-formed state.
 */
import request from "supertest";
import { NscpInstance, REST_URL, setupRestNscp } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";

describe("REST tags", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // A real tag producer on Windows; harmless (no tags) elsewhere.
    await nscp.configure({ "/modules": { CheckDisk: "enabled" } });
    await setupRestNscp(nscp);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("can login", async () => {
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
        expect(key).toBeDefined();
      });
  });

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/tags").trustLocalhost(true).expect(403);
  });

  it("is listed in the v2 endpoint index", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.tags_url).toMatch(/\/api\/v2\/tags$/);
      });
  });

  it("serves the module-contributed tag map", async () => {
    await request(REST_URL)
      .get("/api/v2/tags")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(typeof response.body).toBe("object");
        if (onWindows) {
          // CheckDisk published the logical drive list at load.
          expect(response.body.drives).toMatch(/^[a-z]:(,[a-z]:)*$/);
        } else {
          // No producer on this platform: a well-formed empty map.
          expect(response.body).toEqual({});
        }
      });
  });
});
