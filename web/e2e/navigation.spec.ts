import { expect, test } from "@playwright/test";
import { loginViaLocalStorage, mockApi } from "./mock-api";

test.beforeEach(async ({ page }) => {
  await mockApi(page);
  await loginViaLocalStorage(page);
});

// The permanent sidebar drawer (desktop layout) holds the navigation items.
// (The temporary mobile drawer portals to <body>, so scope to the <nav> element.)
const nav = (page: import("@playwright/test").Page) => page.locator("nav");

test("shows all navigation entries in the sidebar", async ({ page }) => {
  await page.goto("/");

  for (const item of [
    "Dashboard",
    "Modules",
    "Settings",
    "Queries",
    "Metrics",
    "Events",
    "Logs",
    "About",
  ]) {
    await expect(nav(page).getByText(item, { exact: true })).toBeVisible();
  }
});

test("navigates to the modules list", async ({ page }) => {
  await page.goto("/");

  await nav(page).getByText("Modules", { exact: true }).click();
  await expect(page).toHaveURL(/\/modules$/);
  await expect(page.getByText("CheckDisk")).toBeVisible();
  await expect(page.getByText("Monitors disk usage")).toBeVisible();
  await expect(page.getByText("WEBServer")).toBeVisible();
});

test("filters the modules list", async ({ page }) => {
  await page.goto("/modules");

  await expect(page.getByText("CheckDisk")).toBeVisible();
  await page.getByPlaceholder("Filter modules").fill("disk");

  await expect(page.getByText("CheckDisk")).toBeVisible();
  await expect(page.getByText("CheckSystem")).toHaveCount(0);
  await expect(page.getByText("1/3")).toBeVisible();
});

test("navigates to the queries page with queries and aliases", async ({ page }) => {
  await page.goto("/");

  await nav(page).getByText("Queries", { exact: true }).click();
  await expect(page).toHaveURL(/\/queries$/);
  await expect(page.getByText("check_cpu", { exact: true })).toBeVisible();
  await expect(page.getByText("check_drivesize", { exact: true })).toBeVisible();
  await expect(page.getByText("alias_cpu", { exact: true })).toBeVisible();
  await expect(page.getByText("Queries (2)")).toBeVisible();
  await expect(page.getByText("Aliases (1)")).toBeVisible();
});

test("navigates to the logs page and renders log entries", async ({ page }) => {
  await page.goto("/");

  await nav(page).getByText("Logs", { exact: true }).click();
  await expect(page).toHaveURL(/\/logs$/);
  await expect(page.getByText("Service started")).toBeVisible();
  await expect(page.getByText("Sample error entry")).toBeVisible();
});

test("navigates to the about page", async ({ page }) => {
  await page.goto("/");

  await nav(page).getByText("About", { exact: true }).click();
  await expect(page).toHaveURL(/\/about$/);
  await expect(page.getByText("Third-party components")).toBeVisible();
  await expect(page.getByText("Licensed under Apache-2.0 OR GPL-2.0-only.")).toBeVisible();
});

test("supports deep links straight into a sub-page", async ({ page }) => {
  // The SPA fallback must serve index.html for nested routes on a cold load.
  await page.goto("/queries");
  await expect(page.getByText("check_cpu", { exact: true })).toBeVisible();
});

test("logs out from the account menu and returns to the login screen", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByText("CPU Load")).toBeVisible();

  await page.getByRole("button", { name: "account of current user" }).click();
  await page.getByRole("menuitem", { name: "Logout" }).click();

  await expect(page.getByRole("heading", { name: "Sign in" })).toBeVisible();
});
