/**
 * Diagnostics: small point-in-time tables the core collects on an interval
 * and keeps with each failed check, served by WEBServer.
 *
 *   GET /api/v2/diagnostics             the sections, without rows   (diagnostics.list)
 *   GET /api/v2/diagnostics/<section>   one section with its rows    (diagnostics.list)
 *   GET /api/v2/failures/<index>/context
 *                                       gains `diagnostics`          (failures.list)
 *
 * Nothing is collected unless [/settings/core/failed checks] `capture
 * diagnostics` is on, and nothing is collected because a check failed or a
 * request arrived: both read what the last round left. The core always adds
 * `failures.recent`; the process tables come from CheckSystem (Windows) and
 * CheckSystemUnix once [/settings/system/<os>/diagnostics] processes is on.
 * Rounds run with the 1s metrics round here, so every wait below polls with a
 * deadline rather than sleeping a fixed time.
 */
import request from "supertest";

import { NscpInstance, REST_URL, eventIdAllocator, resolveEventLogSource, writeEventLogEntry } from "@fixtures/index";

jest.setTimeout(300_000);

type Json = Record<string, unknown>;

const onWindows = process.platform === "win32";

function config(
  failed: Record<string, string>,
  extra: Record<string, Record<string, string>> = {},
): Record<string, Record<string, string>> {
  return {
    "/settings/core": { "metrics interval": "1s" },
    "/modules": {
      WEBServer: "enabled",
      CheckHelpers: "enabled",
      CheckSystem: "enabled",
      // The event tables on Windows; the journal ones are CheckSystemUnix's.
      ...(onWindows ? { CheckEventLog: "enabled" } : {}),
    },
    "/settings/default": {
      password: "default-password",
      "allowed hosts": "127.0.0.1,::1",
    },
    "/settings/core/failed checks": failed,
    "/settings/WEB/server/roles": {
      full: "*",
      // Reads the failed-check log (and so a failure's diagnostics), not the live tables.
      reader: "failures.list,login.get",
      // Reads the live tables.
      diagnostics: "diagnostics.list,login.get",
    },
    "/settings/WEB/server/users/admin": { role: "full", password: "default-password" },
    "/settings/WEB/server/users/reader": { role: "reader", password: "reader-password" },
    "/settings/WEB/server/users/diag": { role: "diagnostics", password: "diag-password" },
    ...extra,
  };
}

async function login(user: string, password: string): Promise<string> {
  const response = await request(REST_URL)
    .get("/api/v2/login")
    .auth(user, password)
    .trustLocalhost(true)
    .expect(200);
  return response.body.key as string;
}

async function startAgent(
  failed: Record<string, string>,
  extra: Record<string, Record<string, string>> = {},
): Promise<{ nscp: NscpInstance; key: string }> {
  const nscp = new NscpInstance();
  await nscp.configure(config(failed, extra));
  await nscp.waitForPortFree(8443, { timeoutMs: 30_000 });
  nscp.start();
  await nscp.waitForPort(8443, { timeoutMs: 30_000 });
  return { nscp, key: await login("admin", "default-password") };
}

async function get(key: string, path: string, status = 200): Promise<Json> {
  const response = await request(REST_URL)
    .get(path)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(status);
  return response.body as Json;
}

async function run(key: string, command: string, args = ""): Promise<void> {
  await request(REST_URL)
    .get(`/api/v2/queries/${command}/commands/execute${args}`)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
}

/** Poll `probe` until it returns a value, or fail after `timeoutMs`. */
async function until<T>(
  what: string,
  probe: () => Promise<T | undefined>,
  timeoutMs = 60_000,
): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const value = await probe();
    if (value !== undefined) return value;
    if (Date.now() > deadline) throw new Error(`Timed out waiting for ${what}`);
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
}

/** The live section, once a round has produced it with at least `rows` rows. */
async function section(key: string, name: string, rows = 1): Promise<Json> {
  return until(`section ${name}`, async () => {
    const response = await request(REST_URL)
      .get(`/api/v2/diagnostics/${name}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    if (response.status !== 200) return undefined;
    const body = response.body as Json;
    return (body.rows as Json[]).length >= rows ? body : undefined;
  });
}

/** `captured_at` is an ISO timestamp from the last few minutes, not the future. */
function expectRecent(capturedAt: unknown): void {
  expect(capturedAt as string).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$/);
  const at = Date.parse(capturedAt as string);
  expect(at).toBeGreaterThan(Date.now() - 10 * 60_000);
  expect(at).toBeLessThan(Date.now() + 60_000);
}

function expectColumns(body: Json, expected: [string, string][]): void {
  const columns = (body.columns as Json[]).map((c) => [c.name, c.type]);
  expect(columns).toEqual(expected);
  for (const row of body.rows as Json[]) {
    expect(Object.keys(row).sort()).toEqual(expected.map(([name]) => name).sort());
  }
}

function expectProcessTable(body: Json, sortedBy: "cpu" | "working_set"): void {
  expect(typeof body.source).toBe("string");
  expect(body.error).toBeUndefined();
  const columns = (body.columns as Json[]).map((c) => [c.name, c.type, c.unit ?? ""]);
  expect(columns).toEqual([
    ["name", "string", ""],
    ["pid", "int", ""],
    ["cpu", "float", "percent"],
    ["working_set", "int", "bytes"],
    ["private_bytes", "int", "bytes"],
  ]);
  const rows = body.rows as Json[];
  expect(rows.length).toBeGreaterThan(0);
  // The row cap of the agent under test.
  expect(rows.length).toBeLessThanOrEqual(5);
  for (const row of rows) {
    expect(typeof row.name).toBe("string");
    expect(row.pid as number).toBeGreaterThan(0);
    expect(row.cpu as number).toBeGreaterThanOrEqual(0);
    expect(row.working_set as number).toBeGreaterThanOrEqual(0);
    // No command lines, ever.
    expect(Object.keys(row).sort()).toEqual(["cpu", "name", "pid", "private_bytes", "working_set"]);
  }
  const values = rows.map((r) => r[sortedBy] as number);
  expect(values).toEqual([...values].sort((a, b) => b - a));
}

describe("REST diagnostics", () => {
  describe("capture diagnostics off (the default)", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      ({ nscp, key } = await startAgent({}));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("requires authentication", async () => {
      await request(REST_URL).get("/api/v2/diagnostics").trustLocalhost(true).expect(403);
    });

    it("is listed in the v2 endpoint index", async () => {
      const body = await get(key, "/api/v2");
      expect(body.diagnostics_url).toMatch(/\/api\/v2\/diagnostics$/);
    });

    it("answers that diagnostics are disabled", async () => {
      const body = await get(key, "/api/v2/diagnostics");
      expect(body.enabled).toBe(false);
      expect(body.sections).toEqual([]);
      expect(body.snapshot_id).toEqual(0);
      const response = await request(REST_URL)
        .get("/api/v2/diagnostics/failures.recent")
        .set("Authorization", `Bearer ${key}`)
        .trustLocalhost(true)
        .expect(404);
      expect(response.text).toMatch(/disabled/);
    });

    it("records failures without diagnostics", async () => {
      await run(key, "check_critical", "?message=off");
      const log = await get(key, "/api/v2/failures");
      expect(log.capture_diagnostics).toBe(false);
      const entry = (log.entries as Json[])[0];
      expect(entry.diagnostics).toEqual({ state: "disabled" });
      const context = await get(key, `/api/v2/failures/${entry.index}/context`);
      expect((context.diagnostics as Json).state).toEqual("disabled");
      expect((context.diagnostics as Json).sections).toBeUndefined();
    });
  });

  describe("capture diagnostics on, every table on", () => {
    let nscp: NscpInstance;
    let key = "";

    beforeAll(async () => {
      const diagnosticsPath = onWindows
        ? "/settings/system/windows/diagnostics"
        : "/settings/system/unix/diagnostics";
      ({ nscp, key } = await startAgent(
        { "capture diagnostics": "true", "diagnostics rows": "5" },
        {
          [diagnosticsPath]: { processes: "true", services: "true" },
          ...(onWindows ? { "/settings/eventlog/diagnostics": { services: "true", errors: "true" } } : {}),
        },
      ));
    });

    afterAll(async () => {
      await nscp?.stop();
    });

    it("keeps the grant to itself: no failures.list holder reads the live tables", async () => {
      const reader = await login("reader", "reader-password");
      await get(reader, "/api/v2/diagnostics", 403);
      await get(reader, "/api/v2/diagnostics/failures.recent", 403);
      const diag = await login("diag", "diag-password");
      expect((await get(diag, "/api/v2/diagnostics")).enabled).toBe(true);
      // ...and diagnostics.list is not failures.list.
      await get(diag, "/api/v2/failures", 403);
    });

    it("lists the sections of the latest snapshot", async () => {
      const body = await until("a snapshot", async () => {
        const b = await get(key, "/api/v2/diagnostics");
        return (b.snapshot_id as number) > 0 ? b : undefined;
      });
      expect(body.enabled).toBe(true);
      expect(body.max_rows).toEqual(5);
      expect(typeof body.captured_at).toBe("string");
      const sections = body.sections as Json[];
      const recent = sections.find((s) => s.name === "failures.recent") as Json;
      expect(recent.source).toEqual("core");
      // A listing carries the row count, never the rows.
      expect(typeof recent.rows).toBe("number");
      expect(recent.columns).toBeUndefined();
    });

    it("answers 404 for a section the snapshot does not hold", async () => {
      await get(key, "/api/v2/diagnostics/no.such.section", 404);
    });

    it("lists the last failed checks in failures.recent", async () => {
      await run(key, "check_critical", "?message=first%20line%0Asecond%20line");
      const body = await until("the failure in failures.recent", async () => {
        const s = await section(key, "failures.recent");
        const rows = s.rows as Json[];
        return rows.some((r) => r.message === "first line") ? s : undefined;
      });
      const row = (body.rows as Json[]).find((r) => r.message === "first line") as Json;
      expect(row.command).toEqual("check_critical");
      expect(row.status).toEqual("CRITICAL");
      expect(row.subject).toEqual("WEBServer:admin");
      expect(row.time as string).toMatch(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$/);
    });

    it("publishes the top processes by CPU and by memory", async () => {
      expectProcessTable(await section(key, "processes.top_cpu"), "cpu");
      expectProcessTable(await section(key, "processes.top_memory"), "working_set");
    });

    it("publishes the services in trouble (services.problems)", async () => {
      // A healthy machine has none: the table exists, with its columns, and
      // says when it was read. Rows are only checked for their shape.
      if (!onWindows) return;
      const body = await section(key, "services.problems", 0);
      expect(body.error).toBeUndefined();
      expectRecent(body.captured_at);
      expectColumns(body, [
        ["name", "string"],
        ["state", "string"],
        ["exit_code", "int"],
        ["binary_path", "string"],
        ["account", "string"],
        ["started", "timestamp"],
        ["restart_count", "int"],
      ]);
      for (const row of body.rows as Json[]) {
        expect(typeof row.name).toBe("string");
        expect(typeof row.state).toBe("string");
        // Never a command line: an executable has no arguments after it.
        if (onWindows) {
          expect(row.binary_path as string).not.toMatch(/\.exe\s/i);
          // The service control manager keeps no restart count.
          expect(row.restart_count).toBeNull();
        }
      }
    });

    it("publishes the service crash events (services.events)", async () => {
      // A healthy machine may have none in the last ten minutes.
      if (!onWindows) return;
      const body = await section(key, "services.events", 0);
      expect(body.error).toBeUndefined();
      expectRecent(body.captured_at);
      expectColumns(body, [
        ["time", "timestamp"],
        ["service", "string"],
        ["id", "int"],
        ["message", "string"],
      ]);
      for (const row of body.rows as Json[]) {
        expect([7031, 7034, 7024]).toContain(row.id);
        expect(typeof row.service).toBe("string");
      }
    });

    it("publishes the last errors written to the logs (eventlog.errors)", async () => {
      if (!onWindows) return;
      const body = await section(key, "eventlog.errors", 0);
      expect(body.error).toBeUndefined();
      expectRecent(body.captured_at);
      expectColumns(body, [
        ["time", "timestamp"],
        ["log", "string"],
        ["source", "string"],
        ["id", "int"],
        ["level", "string"],
        ["message", "string"],
      ]);

      // Write an error of our own and watch it arrive. The source has no
      // message for our id, so the message is the event's own string; the
      // level and log are the agent's words, not the host's language.
      const source = resolveEventLogSource();
      const id = eventIdAllocator(20000).base;
      const marker = `nscp-diagnostics-${id}`;
      expect(writeEventLogEntry(source, id, "Error", marker)).toBe(true);
      const row = await until("the injected error in eventlog.errors", async () => {
        const s = await section(key, "eventlog.errors", 0);
        return (s.rows as Json[]).find((r) => r.id === id && r.source === source);
      });
      expect(row.log).toEqual("Application");
      expect(row.level).toEqual("error");
      expect(row.message as string).toContain(marker);
      expectRecent(row.time);
    });

    it("records a failure with the snapshot, readable with failures.list", async () => {
      // Wait for a round that has the process tables, then fail a check.
      await section(key, "processes.top_memory");
      await run(key, "check_critical", "?message=with-diagnostics");
      const log = await get(key, "/api/v2/failures");
      expect(log.capture_diagnostics).toBe(true);
      expect(log.diagnostics_rows).toEqual(5);
      const entry = (log.entries as Json[]).find((e) => e.message === "with-diagnostics") as Json;
      const ref = entry.diagnostics as Json;
      expect(ref.state).toEqual("captured");
      expect(typeof ref.snapshot_id).toBe("number");
      expect(ref.sections).toBeUndefined();

      const reader = await login("reader", "reader-password");
      const context = await get(reader, `/api/v2/failures/${entry.index}/context`);
      const diagnostics = context.diagnostics as Json;
      expect(diagnostics.state).toEqual("captured");
      expect(diagnostics.snapshot_id).toEqual(ref.snapshot_id);
      const sections = diagnostics.sections as Record<string, Json>;
      expect(Object.keys(sections)).toEqual(
        expect.arrayContaining(["failures.recent", "processes.top_cpu", "processes.top_memory"]),
      );
      expectProcessTable(sections["processes.top_memory"], "working_set");
    });
  });
});
