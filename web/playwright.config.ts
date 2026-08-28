import { defineConfig } from "@playwright/test";
import fs from "node:fs";

// Some sandboxes ship a pre-installed Chromium instead of the exact build this
// Playwright version would download. Prefer an explicit override, fall back to
// the well-known shared location, and otherwise let Playwright resolve its own
// managed browser.
const chromiumOverride =
  process.env.PLAYWRIGHT_CHROMIUM_EXECUTABLE ||
  (fs.existsSync("/opt/pw-browsers/chromium") ? "/opt/pw-browsers/chromium" : undefined);

export default defineConfig({
  testDir: "./e2e",
  timeout: 30_000,
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 2 : 0,
  reporter: [["list"]],
  use: {
    baseURL: "http://127.0.0.1:4173",
    ...(chromiumOverride ? { launchOptions: { executablePath: chromiumOverride } } : {}),
  },
  webServer: {
    // Serve the production bundle the way it ships; /api never resolves here —
    // every spec installs its own route mocks before loading a page.
    command: "npm run build && npm run preview -- --port 4173 --strictPort",
    url: "http://127.0.0.1:4173",
    reuseExistingServer: !process.env.CI,
    timeout: 240_000,
  },
});
