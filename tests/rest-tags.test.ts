/**
 * REST host-tags scenarios: /api/v2/tags serves the central tag repository —
 * key=value facts contributed by modules through the tag API (NSAPISetTag).
 *
 * Uses a hand-rolled config (not setupRestNscp, which pins CheckSystem to
 * disabled) so both producers run:
 *  - CheckDisk publishes `drives=c:,d:,...` on Windows.
 *  - CheckSystem publishes the host facts (os_name, os_version, os_family,
 *    arch, cpu_cores, memory_gb, virtualization, and - where the host has
 *    them - manufacturer, model and domain) plus the configured service-tags:
 *    EventLog (always running on Windows) maps to `eventlog-service=enabled`,
 *    and a nonexistent service maps to a tag that must NOT appear.
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

  // Host facts. Asserted on shape rather than on this machine's values, and
  // split from the tags above because these are the ones that have to read
  // the same on every platform - a Windows agent and a Linux agent in one
  // fleet are selected with the same expression.
  it("publishes the host facts every platform can answer", async () => {
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
        // Every machine has at least one core and at least a gigabyte, and
        // both are published as plain integers (the tag map is string-valued).
        expect(Number(response.body.cpu_cores)).toBeGreaterThan(0);
        expect(Number(response.body.memory_gb)).toBeGreaterThan(0);
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

  it("omits a fact it could not determine rather than inventing one", async () => {
    await request(REST_URL)
      .get("/api/v2/tags")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        // manufacturer/model come from SMBIOS and domain from the host name,
        // and a VM or an unjoined host legitimately has none of them. What
        // must never happen is a placeholder standing in for the answer.
        for (const fact of ["manufacturer", "model", "domain"]) {
          if (response.body[fact] !== undefined) {
            expect(response.body[fact]).not.toEqual("");
            expect(response.body[fact].toLowerCase()).not.toContain("to be filled by");
            expect(response.body[fact].toLowerCase()).not.toEqual("unknown");
          }
        }
      });
  });
});
