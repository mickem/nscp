/**
 * REST host-tags scenarios: /api/v2/tags serves the central tag repository —
 * key=value facts contributed by modules through the tag API (NSAPISetTag).
 *
 * Uses a hand-rolled config (not setupRestNscp, which pins CheckSystem to
 * disabled) so both producers run:
 *  - CheckDisk publishes `drives=c:,d:,...` on Windows.
 *  - CheckSystem publishes `os_version`/`os_name` plus the configured
 *    service-tags: EventLog (always running on Windows) maps to
 *    `eventlog-service=enabled`, and a nonexistent service maps to a tag
 *    that must NOT appear.
 * On Linux the CheckSystem module resolves to the unix variant whose
 * service-tags check systemd units; the mapped names don't exist there, so
 * the tags must stay absent — asserted, since "no tag" is the documented
 * contract for a missing/stopped service.
 */
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";

describe("REST tags", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckDisk: "enabled",
        CheckSystem: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      // Windows service / systemd unit -> tag mappings. EventLog always runs
      // on Windows; the ghost entry proves absent services publish nothing.
      [`/settings/system/${onWindows ? "windows" : "unix"}/service-tags`]: {
        EventLog: "eventlog-service",
        NoSuchServiceXyz: "ghost",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
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
        // A service that does not exist must never publish its tag.
        expect(response.body.ghost).toBeUndefined();
        if (onWindows) {
          // CheckDisk published the logical drive list at load.
          expect(response.body.drives).toMatch(/^[a-z]:(,[a-z]:)*$/);
          // CheckSystem published the Windows version...
          expect(response.body.os_version).toMatch(/^\d+\.\d+\.\d+$/);
          expect(response.body.os_name).toContain("Windows");
          // ...and the configured service-tag for a running service.
          expect(response.body["eventlog-service"]).toEqual("enabled");
        } else {
          // The EventLog systemd unit does not exist on Linux either.
          expect(response.body["eventlog-service"]).toBeUndefined();
        }
      });
  });
});
