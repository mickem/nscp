import type { Page, Route } from "@playwright/test";

/** Credentials accepted by the mocked login endpoint. */
export const E2E_USER = "admin";
export const E2E_PASSWORD = "s3cret";
export const E2E_TOKEN = "e2e-session-token";

export const METRICS_FIXTURE: Record<string, string | number> = {
  "system.cpu.total.kernel": 12.5,
  "system.cpu.total.user": 30.1,
  "system.mem.physical.used": 4 * 1024 ** 3,
  "system.mem.physical.total": 16 * 1024 ** 3,
  "system.uptime.uptime": "3d 2:07",
  "system.uptime.boot": "2026-08-24 09:00",
  "system.metrics.procs.procs": 214,
  "system.metrics.procs.threads": 1890,
  "workers.jobs": 7,
  "workers.submitted": 421,
  "workers.errors": 0,
  "workers.refresh_interval": 10,
  "system.refresh_interval": 10,
  "disk.free.C:.total": 100 * 1024 ** 3,
  "disk.free.C:.free": 40 * 1024 ** 3,
  "disk.free.C:.used": 60 * 1024 ** 3,
  "disk.free.C:.used_pct": 60,
};

export const TAGS_FIXTURE = { drives: "C:,D:", os: "windows" };

const moduleFixture = (id: string, loaded: boolean, enabled: boolean, description: string) => ({
  id,
  name: id,
  title: id,
  description,
  enabled,
  loaded,
  metadata: { alias: "", plugin_id: "0" },
  load_url: "",
  unload_url: "",
  module_url: "",
});

export const MODULES_FIXTURE = [
  moduleFixture("CheckDisk", true, true, "Monitors disk usage"),
  moduleFixture("CheckSystem", true, true, "Monitors cpu, memory and processes"),
  moduleFixture("WEBServer", false, false, "Serves this web interface"),
];

export const QUERIES_FIXTURE = [
  {
    name: "check_cpu",
    title: "check_cpu",
    plugin: "CheckSystem",
    description: "Check the CPU load",
    query_url: "",
  },
  {
    name: "check_drivesize",
    title: "check_drivesize",
    plugin: "CheckDisk",
    description: "Check free disk space",
    query_url: "",
  },
];

export const ALIASES_FIXTURE = [
  {
    name: "alias_cpu",
    title: "alias_cpu",
    plugin: "CheckSystem",
    description: "Alias for check_cpu",
    alias_url: "",
  },
];

export const LOGS_FIXTURE = [
  {
    date: "2026-08-27 10:00:00",
    file: "core.cpp",
    level: "info",
    line: 1,
    message: "Service started",
  },
  {
    date: "2026-08-27 10:00:05",
    file: "web.cpp",
    level: "error",
    line: 42,
    message: "Sample error entry",
  },
];

type JsonBody = unknown;
type Overrides = Record<string, JsonBody | ((route: Route) => Promise<void> | void)>;

function json(route: Route, body: JsonBody, headers: Record<string, string> = {}) {
  return route.fulfill({
    status: 200,
    contentType: "application/json",
    headers,
    body: JSON.stringify(body),
  });
}

/** The metrics snapshot entry 2 was recorded with, as /api/v2/failures/2/context serves it. */
const FAILURE_CONTEXT_FIXTURE = {
  index: 2,
  timestamp: 1760000060,
  state: "captured",
  snapshot_id: 7,
  captured_at: 1760000055,
  age_at_failure: 5,
  size: 2048,
  metrics: { "system.cpu.total.idle": 3, "system.mem.physical.used": 15032385536 },
  metadata: {
    "system.cpu.total.idle": { type: "gauge", unit: "percent" },
    "system.mem.physical.used": { type: "gauge", unit: "bytes" },
  },
};

/** Two failed checks, newest first, as /api/v2/failures serves them. */
const FAILURES_FIXTURE = {
  enabled: true,
  max_entries: 50,
  arguments_recorded: true,
  arguments_mode: "safe",
  statuses: "warning,critical,unknown",
  last_index: 2,
  dropped: 0,
  capture_metrics: true,
  context_budget: 4194304,
  max_context_size: 1048576,
  context_bytes_used: 2048,
  capture_diagnostics: true,
  diagnostics_rows: 10,
  count: 2,
  entries: [
    {
      index: 2,
      timestamp: 1760000060,
      date: "2025-Oct-09 10:01:00",
      age: 30,
      subject: "WEBServer:admin",
      depth: 0,
      command: "check_drivesize",
      alias: "",
      status: 2,
      result: "CRITICAL",
      message: "CRITICAL C: 92% used",
      lines: [
        {
          message: "CRITICAL C: 92% used",
          perf: { "C: used %": { value: 92, unit: "%", warning: 80, critical: 90, minimum: 0, maximum: 100 } },
        },
      ],
      arguments: ["drive=C:", "warning=used > 80%", "critical=used > 90%"],
      context: { state: "captured", snapshot_id: 7, captured_at: 1760000055, age_at_failure: 5, size: 2048 },
      diagnostics: { state: "captured", snapshot_id: 8, captured_at: 1760000057, age_at_failure: 3, size: 900 },
    },
    {
      index: 1,
      timestamp: 1760000000,
      date: "2025-Oct-09 10:00:00",
      age: 90,
      subject: "NRPEServer",
      depth: 0,
      command: "check_cpu",
      alias: "",
      status: 1,
      result: "WARNING",
      message: "WARNING: 5m: 85%",
      lines: [
        { message: "WARNING: 5m: 85%", perf: { "total 5m": { value: 85, unit: "%", warning: 80, critical: 90 } } },
      ],
      arguments: [],
    },
  ],
};

const TOP_CPU_SECTION = {
  source: "CheckSystem",
  description: "The processes using the most CPU over the last second",
  captured_at: "2025-10-09T10:00:56Z",
  truncated: true,
  columns: [
    { name: "name", type: "string" },
    { name: "pid", type: "int" },
    { name: "cpu", type: "float", unit: "percent", description: "Share of all cores over the last second" },
    { name: "working_set", type: "int", unit: "bytes", description: "Resident memory" },
    { name: "private_bytes", type: "int", unit: "bytes", description: "Memory not shared with other processes" },
  ],
  rows: [
    { name: "acme-indexer.exe", pid: 4242, cpu: 87.5, working_set: 412000256, private_bytes: 398458880 },
    { name: "svchost.exe", pid: 1288, cpu: 2.1, working_set: 85983232, private_bytes: null },
  ],
};

const RECENT_FAILURES_SECTION = {
  source: "core",
  description: "The last failed checks, newest first",
  captured_at: "2025-10-09T10:00:57Z",
  truncated: false,
  columns: [
    { name: "time", type: "timestamp" },
    { name: "command", type: "string" },
    { name: "status", type: "string" },
    { name: "subject", type: "string" },
    { name: "message", type: "string" },
  ],
  rows: [
    {
      time: "2025-10-09T10:00:00Z",
      command: "check_cpu",
      status: "WARNING",
      subject: "NRPEServer",
      message: "WARNING: 5m: 85%",
    },
  ],
};

/** The diagnostics entry 2 was recorded with, inside its /context. */
const FAILURE_DIAGNOSTICS_FIXTURE = {
  state: "captured",
  snapshot_id: 8,
  captured_at: 1760000057,
  age_at_failure: 3,
  size: 900,
  sections: {
    "processes.top_cpu": TOP_CPU_SECTION,
    "failures.recent": RECENT_FAILURES_SECTION,
  },
};

/** The live snapshot, as /api/v2/diagnostics lists it. */
const DIAGNOSTICS_FIXTURE = {
  enabled: true,
  snapshot_id: 812,
  captured_at: "2025-10-09T10:00:57Z",
  max_rows: 10,
  dropped: 0,
  excluded: ["eventlog.errors"],
  count: 2,
  sections: [
    { name: "processes.top_cpu", ...TOP_CPU_SECTION, columns: undefined, rows: 2 },
    { name: "failures.recent", ...RECENT_FAILURES_SECTION, columns: undefined, rows: 1 },
  ],
};

/**
 * Intercept every /api call the UI makes and answer with deterministic
 * fixtures, so the rendering tests never need a live NSClient++ backend.
 * `overrides` maps a pathname (e.g. "/api/v2/metrics") to a replacement JSON
 * body or a custom route handler.
 */
export async function mockApi(page: Page, overrides: Overrides = {}) {
  await page.route("**/api/**", async (route) => {
    const path = new URL(route.request().url()).pathname;

    const override = overrides[path];
    if (override !== undefined) {
      if (typeof override === "function") {
        await override(route);
      } else {
        await json(route, override);
      }
      return;
    }

    switch (path) {
      case "/api/v2/login": {
        const auth = route.request().headers()["authorization"] || "";
        // Logging out revokes the bearer server-side before the client forgets
        // it, so the same route answers DELETE with the token it issued.
        if (route.request().method() === "DELETE") {
          if (auth === `Bearer ${E2E_TOKEN}`) {
            return json(route, {});
          }
          return route.fulfill({ status: 403, body: "Forbidden" });
        }
        const expected = `Basic ${Buffer.from(`${E2E_USER}:${E2E_PASSWORD}`).toString("base64")}`;
        if (auth === expected) {
          return json(route, { key: E2E_TOKEN, user: E2E_USER });
        }
        return route.fulfill({ status: 403, body: "Forbidden" });
      }
      case "/api/v2/info":
        return json(route, { name: "NSClient++", version: "0.17.0", version_url: "" });
      case "/api/v2/info/version":
        return json(route, { version: "0.17.0" });
      case "/api/v2/metrics":
        return json(route, METRICS_FIXTURE);
      case "/api/v2/tags":
        return json(route, TAGS_FIXTURE);
      case "/api/v2/logs/status":
        return json(route, { errors: 0, last_error: "" });
      case "/api/v2/logs":
        return json(route, LOGS_FIXTURE, {
          "X-Pagination-Count": String(LOGS_FIXTURE.length),
          "X-Pagination-Page": "1",
          "X-Pagination-Limit": "10",
        });
      case "/api/v2/settings/status":
        return json(route, { context: "ini://", type: "ini", has_changed: false });
      case "/api/v2/modules":
        return json(route, MODULES_FIXTURE);
      case "/api/v2/queries":
        return json(route, QUERIES_FIXTURE);
      case "/api/v2/aliases":
        return json(route, ALIASES_FIXTURE);
      case "/api/v2/events":
        return json(route, []);
      case "/api/v2/failures":
        return json(route, FAILURES_FIXTURE);
      case "/api/v2/failures/2/context":
        return json(route, { ...FAILURE_CONTEXT_FIXTURE, diagnostics: FAILURE_DIAGNOSTICS_FIXTURE });
      case "/api/v2/diagnostics":
        return json(route, DIAGNOSTICS_FIXTURE);
      case "/api/v2/diagnostics/processes.top_cpu":
        return json(route, { name: "processes.top_cpu", snapshot_id: 812, ...TOP_CPU_SECTION });
      case "/api/v2/diagnostics/failures.recent":
        return json(route, { name: "failures.recent", snapshot_id: 812, ...RECENT_FAILURES_SECTION });
      default:
        // Fail loudly: an unmocked endpoint means the fixture set is stale.
        return route.fulfill({ status: 404, body: `No e2e mock for ${path}` });
    }
  });
}

/**
 * Seed the persisted token so the app boots straight into the shell.
 *
 * sessionStorage, not localStorage: that is where useAuthentication keeps the
 * bearer, so seeding anywhere else lands on the login screen instead of the
 * shell. Kept in step with TOKEN_STORAGE_KEY in src/common/hooks/auth.ts.
 */
export async function loginViaSessionStorage(page: Page) {
  await page.addInitScript((token: string) => {
    window.sessionStorage.setItem("token", token);
  }, E2E_TOKEN);
}
