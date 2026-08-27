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
      default:
        // Fail loudly: an unmocked endpoint means the fixture set is stale.
        return route.fulfill({ status: 404, body: `No e2e mock for ${path}` });
    }
  });
}

/** Seed the persisted token so the app boots straight into the shell. */
export async function loginViaLocalStorage(page: Page) {
  await page.addInitScript((token: string) => {
    window.localStorage.setItem("token", token);
  }, E2E_TOKEN);
}
