/**
 * REST host-facts scenarios: /api/v2/facts serves the fact repository — the
 * structured, hierarchical inventory modules publish through fetchFacts, as
 * opposed to /api/v2/tags, which serves the flat strings a fleet selector
 * matches on.
 *
 * The contract this pins is the opt-in one: with nothing enabled in
 * [/settings/facts] the endpoint answers with an empty document, an empty
 * enabled list, and the hash of `{}` — it does not 404, and it does not
 * quietly collect anything. That is the state every fresh install is in, so
 * it is the state most worth asserting.
 *
 * Grants: reading is `facts.get` (which the `monitoring` role carries) and
 * refreshing is `facts.refresh` (which only `full` does), because a refresh
 * makes every producer collect now.
 *
 * Uses a hand-rolled config rather than setupRestNscp so the two roles can
 * each have a user.
 */
import request from "supertest";
import { NscpInstance, REST_URL } from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";

// sha256 of "{}" — the canonical serialisation of the empty facts document.
const EMPTY_DOCUMENT_HASH = "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a";

describe("REST facts", () => {
  let nscp: NscpInstance;
  let adminKey: string | undefined = undefined;
  let monitoringKey: string | undefined = undefined;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": {
        WEBServer: "enabled",
      },
      "/settings/default": {
        "allowed hosts": "127.0.0.1,::1",
      },
      "/settings/WEB/server/users/admin": {
        role: "full",
        password: "default-password",
      },
      "/settings/WEB/server/users/watcher": {
        role: "monitoring",
        password: "watcher-password",
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
        adminKey = response.body.key;
        expect(adminKey).toBeDefined();
      });
    await request(REST_URL)
      .get("/api/v2/login")
      .auth("watcher", "watcher-password")
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        monitoringKey = response.body.key;
        expect(monitoringKey).toBeDefined();
      });
  });

  it("requires authentication", async () => {
    await request(REST_URL).get("/api/v2/facts").trustLocalhost(true).expect(403);
  });

  it("is listed in the v2 endpoint index", async () => {
    await request(REST_URL)
      .get("/api/v2")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts_url).toMatch(/\/api\/v2\/facts$/);
      });
  });

  it("serves an empty document when no fact set is enabled", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts).toEqual({});
        expect(response.body.enabled).toEqual([]);
        expect(response.body.errors).toEqual({});
        expect(response.body.revision).toEqual(0);
        // The empty document still has a hash: that is what lets a fleet
        // server tell "inventory switched off" from "agent too old to have
        // any".
        expect(response.body.hash).toEqual(EMPTY_DOCUMENT_HASH);
      });
  });

  it("tags the response with the document hash so a poller can revalidate", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.headers.etag).toEqual(`"${EMPTY_DOCUMENT_HASH}"`);
      });
  });

  it("answers 404 for a path nothing produced", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/os")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(404);
  });

  it("rejects a path that is not a dotted fact path", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/Not_A_Path")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(400);
  });

  it("lets the monitoring role read but not refresh", async () => {
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${monitoringKey}`)
      .trustLocalhost(true)
      .expect(200);
    await request(REST_URL)
      .post("/api/v2/facts/refresh")
      .set("Authorization", `Bearer ${monitoringKey}`)
      .trustLocalhost(true)
      .expect(403);
  });

  it("refreshes on demand and returns the same envelope", async () => {
    await request(REST_URL)
      .post("/api/v2/facts/refresh")
      .set("Authorization", `Bearer ${adminKey}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.facts).toEqual({});
        expect(response.body.enabled).toEqual([]);
        // A round that collects nothing must not move the revision: a poller
        // watching it would otherwise see a change on every refresh.
        expect(response.body.revision).toEqual(0);
        expect(response.body.hash).toEqual(EMPTY_DOCUMENT_HASH);
      });
  });
});

/**
 * The wave-1 producers, with every set they own enabled. What is asserted is
 * only what is true of any machine CI runs on - that the sets arrive, that
 * the documented keys are there, and that every record carries the stable id
 * the fleet server diffs lists by. Anything host-specific (a drive letter, a
 * vendor name) would make this a test of the runner rather than of the code.
 */
describe("REST facts with the wave-1 producers enabled", () => {
  let nscp: NscpInstance;
  let key: string | undefined = undefined;
  let facts: Record<string, any> = {};

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
      "/settings/facts": {
        agent: "true",
        os: "true",
        identity: "true",
        hardware: "true",
        "network.interfaces": "true",
        "storage.volumes": "true",
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
    await request(REST_URL)
      .get("/api/v2/facts")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        facts = response.body;
      });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("reports every enabled set and a document that is no longer empty", () => {
    expect(facts.enabled.sort()).toEqual([
      "agent",
      "hardware",
      "identity",
      "network.interfaces",
      "os",
      "storage.volumes",
    ]);
    expect(facts.revision).toBeGreaterThan(0);
    expect(facts.hash).not.toEqual(EMPTY_DOCUMENT_HASH);
    expect(facts.errors).toEqual({});
  });

  it("describes the agent itself", () => {
    expect(typeof facts.facts.agent.version).toBe("string");
    expect(Array.isArray(facts.facts.agent.modules)).toBe(true);
    expect(facts.facts.agent.modules).toContain("CheckDisk");
    // A host that has not enrolled must not claim it has.
    expect(facts.facts.agent.enrolled).toBe(false);
  });

  it("describes the operating system with the same words on both platforms", () => {
    const os = facts.facts.os;
    expect(os.family).toEqual(onWindows ? "windows" : "linux");
    expect(typeof os.name).toBe("string");
    expect(os.name.length).toBeGreaterThan(0);
    expect(typeof os.arch).toBe("string");
    // ISO 8601 UTC, which is what every timestamp in the document is.
    expect(os.boot_time).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/);
  });

  it("describes the host identity", () => {
    expect(typeof facts.facts.identity.hostname).toBe("string");
    expect(facts.facts.identity.hostname.length).toBeGreaterThan(0);
  });

  it("describes the hardware with what the platform could read", () => {
    // CPU and memory are readable without privileges on both platforms; the
    // DMI serial and UUID are not, so they are not asserted.
    expect(facts.facts.hardware.memory.total_bytes).toBeGreaterThan(0);
    expect(facts.facts.hardware.cpu.cores).toBeGreaterThan(0);
  });

  it("lists every volume as a record identified by its mount point or drive", () => {
    const volumes = facts.facts.storage.volumes;
    expect(Array.isArray(volumes)).toBe(true);
    expect(volumes.length).toBeGreaterThan(0);
    const ids = new Set<string>();
    for (const volume of volumes) {
      expect(typeof volume.id).toBe("string");
      expect(volume.id.length).toBeGreaterThan(0);
      // Without a unique id the server cannot diff the list, and the core
      // would have rejected the whole set.
      expect(ids.has(volume.id)).toBe(false);
      ids.add(volume.id);
      expect(typeof volume.type).toBe("string");
    }
    if (onWindows) {
      expect([...ids].some((id) => /^[a-zA-Z]:$/.test(id))).toBe(true);
    } else {
      expect(ids.has("/")).toBe(true);
    }
  });

  it("lists every network interface as a record identified by its name", () => {
    const interfaces = facts.facts.network.interfaces;
    expect(Array.isArray(interfaces)).toBe(true);
    expect(interfaces.length).toBeGreaterThan(0);
    const ids = new Set<string>();
    for (const nic of interfaces) {
      expect(typeof nic.id).toBe("string");
      expect(nic.id.length).toBeGreaterThan(0);
      expect(ids.has(nic.id)).toBe(false);
      ids.add(nic.id);
      // The same three words on both platforms, or a fleet query over them
      // means nothing.
      expect(["up", "down", "unknown"]).toContain(nic.state);
      if (nic.mac !== undefined) expect(nic.mac).toMatch(/^([0-9a-f]{2}:)+[0-9a-f]{2}$/);
    }
  });

  it("serves a single set on its own path", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/os.family")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200)
      .then((response) => {
        expect(response.body.found).toBe(true);
        expect(response.body.facts).toEqual(onWindows ? "windows" : "linux");
      });
  });

  it("does not collect a set that was not enabled", async () => {
    await request(REST_URL)
      .get("/api/v2/facts/software")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(404);
  });
});
