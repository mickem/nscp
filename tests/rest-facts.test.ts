/**
 * REST host-facts scenarios: /api/v2/facts serves the inventory the core
 * collects from its producers.
 *
 * Distinct from rest-tags.test.ts next door, and the split is the point. A tag
 * is a flat value a fleet selector matches whole and is always published; a
 * fact set is a document and is collected only once an operator enables it in
 * the module that produces it. CheckSystem is the producer here, with `os` and
 * `hardware` turned on.
 */
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";
const factsSection = `/settings/system/${onWindows ? "windows" : "unix"}/facts`;

describe("REST facts", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckSystem: "enabled",
        CheckDisk: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      [factsSection]: {
        os: "true",
        hardware: "true",
        "network.interfaces": "true",
      },
      "/settings/disk/facts": {
        "storage.volumes": "true",
      },
      "/settings/facts": {
        agent: "true",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/facts").trustLocalhost(true).expect(403);
  });

  it("is listed in the v2 endpoint index", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts_url).toMatch(/\/api\/v2\/facts$/);
      });
  });

  it("serves the sets its producers were configured to produce", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.enabled.sort()).toEqual([
          "agent",
          "hardware",
          "network",
          "os",
          "storage",
        ]);
        expect(response.body.errors).toEqual({});
        expect(response.body.found).toBe(true);
        expect(response.body.revision).toBeGreaterThan(0);
        // ISO 8601 UTC, as the document rules require.
        expect(response.body.collected).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
        // Per set, when its values were actually read off the machine.
        // CheckSystem caches, so this is what says how old the numbers are -
        // `collected` only says when the core last asked.
        // The core's own `agent` set carries none: it is built on the round,
        // so the round's `collected` time is when it was read.
        expect(Object.keys(response.body.gathered).sort()).toEqual([
          "hardware",
          "network",
          "os",
          "storage",
        ]);
        expect(response.body.gathered.os).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);

        expect(response.body.facts.os.family).toEqual(onWindows ? "windows" : "linux");
        expect(response.body.facts.os.name).toBeTruthy();
        expect(response.body.facts.os.version).toBeTruthy();
        expect(response.body.facts.os.arch).toMatch(/^[a-z0-9_]+$/);
        // Numbers cross the wire as numbers here, unlike in the string-valued
        // tag map - that is half the reason the document exists.
        expect(typeof response.body.facts.hardware.cpu_cores).toBe("number");
        expect(response.body.facts.hardware.cpu_cores).toBeGreaterThan(0);
        expect(typeof response.body.facts.hardware.memory_gb).toBe("number");
      });
  });

  it("lists the volumes by the name check_drivesize gives them", async () => {
    const response = await request(REST_URL)
      .get("/api/v2/facts?path=storage.volumes")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.found).toBe(true);
    const volumes = response.body.facts;
    expect(Array.isArray(volumes)).toBe(true);
    for (const volume of volumes) {
      expect(typeof volume.id).toBe("string");
      expect([
        "fixed",
        "remote",
        "removable",
        "cdrom",
        "ramdisk",
        "unknown",
        "no_root_dir",
      ]).toContain(volume.type);
      // Inventory, not monitoring: the size, never the free space, which
      // would change the document every round.
      expect(Object.keys(volume).filter((k) => /free|used/.test(k))).toEqual([]);
      if (volume.size_bytes !== undefined) expect(volume.size_bytes).toBeGreaterThan(0);
    }
    if (onWindows) {
      // The system drive is on every Windows host, named the way
      // check_drivesize names it.
      const system = volumes.find((v: { id: string }) => v.id.toUpperCase() === "C:\\");
      expect(system).toBeDefined();
      expect(system.filesystem).toBeTruthy();
      expect(system.size_bytes).toBeGreaterThan(0);
    } else {
      // Mount points. A container may have no real filesystem at all
      // (overlay is skipped, as check_drivesize drive=* skips it), which is
      // why this cannot insist on "/".
      for (const volume of volumes) expect(volume.id.startsWith("/")).toBe(true);
    }

    // The same ids check_drivesize reports, which is what lets a failing
    // check find its record.
    const check = await request(REST_URL)
      .get(
        "/api/v2/queries/check_drivesize/commands/execute?drive=*&filter=none&warning=none&critical=none&empty-state=ok&top-syntax=${list}&detail-syntax=%25(drive)",
      )
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const reported = check.body.lines.map((l: { message: string }) => l.message).join("");
    for (const volume of volumes) expect(reported).toContain(volume.id);
  });

  it("lists the network interfaces, without the loopback", async () => {
    const response = await request(REST_URL)
      .get("/api/v2/facts?path=network.interfaces")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.found).toBe(true);
    const interfaces = response.body.facts;
    expect(Array.isArray(interfaces)).toBe(true);
    // A host running a REST server has at least one interface besides the
    // loopback.
    expect(interfaces.length).toBeGreaterThan(0);
    const ids = interfaces.map((i: { id: string }) => i.id);
    expect(new Set(ids).size).toEqual(ids.length);
    expect(ids).not.toContain("lo");
    for (const nic of interfaces) {
      // One spelling of a MAC on every platform.
      if (nic.mac !== undefined) expect(nic.mac).toMatch(/^([0-9a-f]{2}:){5}[0-9a-f]{2}$/);
      if (nic.status !== undefined)
        expect([
          "up",
          "down",
          "testing",
          "unknown",
          "dormant",
          "notpresent",
          "lowerlayerdown",
        ]).toContain(nic.status);
      if (nic.speed_bps !== undefined) expect(nic.speed_bps).toBeGreaterThan(0);
      for (const address of nic.addresses ?? []) expect(address).not.toContain("%");
      // No traffic counters: they would change the document every round.
      expect(Object.keys(nic).filter((k) => /bytes|packets|errors/.test(k))).toEqual([]);
    }
  });

  it("describes the agent itself", async () => {
    const response = await request(REST_URL)
      .get("/api/v2/facts?path=agent")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    expect(response.body.found).toBe(true);
    expect(response.body.facts.version).toMatch(/^\d+\.\d+/);
    expect(response.body.facts.modules).toEqual(
      expect.arrayContaining(["CheckDisk", "CheckSystem", "WEBServer"]),
    );
    expect([...response.body.facts.modules].sort()).toEqual(response.body.facts.modules);
    // Yes or no, never which server: this instance was never enrolled.
    expect(response.body.facts.enrolled).toBe(false);
  });

  it("serves a subtree, and says so when nothing produced one", async () => {
    await request(REST_URL)
      .get("/api/v2/facts?path=os.family")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.path).toEqual("os.family");
        expect(response.body.found).toBe(true);
        expect(response.body.facts).toEqual(onWindows ? "windows" : "linux");
      });

    // A set nobody produces is not an error: a UI asking for one an operator
    // has not enabled should render "not collected", not a failure.
    await request(REST_URL)
      .get("/api/v2/facts?path=software")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.found).toBe(false);
        expect(response.body.facts).toEqual({});
      });
  });

  it("collects on demand, and that is what moves the gathered time", async () => {
    // One round to settle first. `nscp test` loads its console module after
    // the startup round, so the first round after that rightly lists one more
    // module in `agent`. From then on nothing about this host changes.
    await request(REST_URL)
      .post("/api/v2/facts/commands/refresh")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);

    const before = await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);

    const refreshed = await request(REST_URL)
      .post("/api/v2/facts/commands/refresh")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);

    // The round answers with the document it produced, so a caller does not
    // have to follow up with a GET.
    expect(refreshed.body.facts.os.family).toEqual(onWindows ? "windows" : "linux");
    // A manual refresh is the reason that makes a cached producer read the
    // machine again, so the values are at least as fresh as they were.
    expect(new Date(refreshed.body.gathered.os).getTime()).toBeGreaterThanOrEqual(
      new Date(before.body.gathered.os).getTime(),
    );
    // Nothing about this host changed, so the document did not either.
    expect(refreshed.body.revision).toEqual(before.body.revision);
  });
});

describe("REST facts with no set enabled", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
        CheckSystem: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
    });
    nscp.start();
    await nscp.waitForPort(8443, { timeoutMs: 30_000 });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("admin", "default-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        key = response.body.key;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("reports an empty inventory rather than an error", async () => {
    // The default: an inventory is data an operator did not necessarily agree
    // to ship, so a fresh install collects nothing and says so plainly.
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.enabled).toEqual([]);
        expect(response.body.facts).toEqual({});
        // Nothing was stored, so the revision never moved off its initial 0.
        // `collected` is when the last round *completed*, and a round with no
        // producer to ask completes like any other - so it carries a
        // timestamp even here.
        expect(response.body.revision).toEqual(0);
      });
  });
});
