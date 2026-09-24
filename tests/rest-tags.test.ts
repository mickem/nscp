/**
 * REST host-tags scenarios: /api/v2/tags serves the central tag repository —
 * key=value facts contributed by modules through the tag API (NSAPISetTag).
 *
 * Uses a hand-rolled config (not setupRestNscp, which pins CheckSystem to
 * disabled) so both producers run:
 *  - CheckDisk publishes `drives=c:,d:,...` on Windows.
 *  - CheckSystem publishes the five selector facts (os_name, os_version,
 *    os_family, arch, virtualization) plus the configured service-tags:
 *    EventLog (always running on Windows) maps to `eventlog-service=enabled`,
 *    and a nonexistent service maps to a tag that must NOT appear. The rest
 *    of what it gathers - the vendor, model, size and domain - is inventory
 *    and goes into the `os`/`hardware` fact sets instead, which are opt-in
 *    and are covered by the producer's unit tests.
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

  // The selector tags. Asserted on shape rather than on this machine's
  // values, and split from the tags above because these are the ones that
  // have to read the same on every platform - a Windows agent and a Linux
  // agent in one fleet are selected with the same expression.
  it("publishes the selector tags every platform can answer", async () => {
    await request(REST_URL)
      .get("/api/v2/tags")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.os_family).toEqual(onWindows ? "windows" : "linux");
        // Not an exhaustive list: an architecture we have not seen is
        // published lower-cased rather than dropped.
        expect(response.body.arch).toMatch(/^[a-z0-9_]+$/);
        expect(response.body.os_name).toBeTruthy();
        expect(response.body.os_version).toBeTruthy();
      });
  });

  it("names the virtualization from a closed vocabulary", async () => {
    await request(REST_URL)
      .get("/api/v2/tags")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        // The fact is absent only where the host could not be asked at all
        // (no CPUID and no DMI); on any CI runner it has to be present, and
        // it must never be an unmapped vendor id.
        expect(response.body.virtualization).toBeDefined();
        expect([
          "none",
          "virtual",
          "vmware",
          "hyperv",
          "kvm",
          "xen",
          "virtualbox",
          "qemu",
          "parallels",
          "bhyve",
          "acrn",
        ]).toContain(response.body.virtualization);
      });
  });

  it("keeps the inventory out of the tags", async () => {
    await request(REST_URL)
      .get("/api/v2/tags")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        // A tag is uploaded to the fleet server on every state report, where
        // a fact set is only collected once an operator turns it on. The
        // hardware identity, the size and the domain are inventory and must
        // not arrive by the other road.
        for (const fact of ["manufacturer", "model", "domain", "cpu_cores", "memory_gb"]) {
          expect(response.body[fact]).toBeUndefined();
        }
      });
  });
});
