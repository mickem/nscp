import { expect, test } from "@playwright/test";
import { loginViaLocalStorage, mockApi } from "./mock-api";

test.beforeEach(async ({ page }) => {
  await mockApi(page);
  await loginViaLocalStorage(page);
});

test("renders every widget the metrics contain data for", async ({ page }) => {
  await page.goto("/");

  await expect(page.getByText("CPU Load")).toBeVisible();
  await expect(page.getByText("Memory Usage")).toBeVisible();
  await expect(page.getByText("Disk Space")).toBeVisible();
  await expect(page.getByText("System Info")).toBeVisible();
  await expect(page.getByText("Tags", { exact: true })).toBeVisible();
});

test("draws real SVG charts for CPU and memory", async ({ page }) => {
  await page.goto("/");

  const cpuCard = page.locator(".MuiCard-root", { hasText: "CPU Load" });
  await expect(cpuCard.locator("svg").first()).toBeVisible();
  // The chart legend proves the series were wired up, not just an empty svg.
  await expect(cpuCard.getByText("Kernel time (%)")).toBeVisible();
  await expect(cpuCard.getByText("User time (%)")).toBeVisible();

  const memCard = page.locator(".MuiCard-root", { hasText: "Memory Usage" });
  await expect(memCard.locator("svg").first()).toBeVisible();
  await expect(memCard.getByText("Memory (%)")).toBeVisible();
});

test("shows system info values from the metrics API", async ({ page }) => {
  await page.goto("/");

  const infoCard = page.locator(".MuiCard-root", { hasText: "System Info" });
  await expect(infoCard.getByRole("row", { name: /Uptime 3d 2:07/ })).toBeVisible();
  await expect(infoCard.getByRole("row", { name: /Processes 214/ })).toBeVisible();
  await expect(infoCard.getByRole("row", { name: /Worker jobs 7/ })).toBeVisible();
});

test("shows the disk usage bar with formatted sizes", async ({ page }) => {
  await page.goto("/");

  const diskCard = page.locator(".MuiCard-root", { hasText: "Disk Space" });
  await expect(diskCard.getByText("C:", { exact: true })).toBeVisible();
  await expect(diskCard.getByText("60%")).toBeVisible();
  await expect(diskCard.getByText("60.0 GB used · 40.0 GB free · 100.0 GB total")).toBeVisible();
  await expect(diskCard.getByRole("progressbar")).toBeVisible();
});

test("shows host tags as chips", async ({ page }) => {
  await page.goto("/");

  await expect(page.getByText("drives: C:,D:")).toBeVisible();
  await expect(page.getByText("os: windows")).toBeVisible();
});

test("offers the refresh rate selector with server-aware options", async ({ page }) => {
  await page.goto("/");

  await page.getByLabel("Refresh rate").click();
  const listbox = page.getByRole("listbox");
  await expect(listbox.getByRole("option", { name: "Off" })).toBeVisible();
  await expect(listbox.getByRole("option", { name: "10 seconds" })).toBeVisible();
  // The server reports a 10s collection interval, so faster rates are disabled
  // (their accessible name becomes the explanatory tooltip, so match by text).
  await expect(listbox.getByRole("option").filter({ hasText: "1 second" })).toBeDisabled();
  await expect(listbox.getByRole("option").filter({ hasText: "5 seconds" })).toBeDisabled();
  await expect(listbox.getByRole("option", { name: "1 minute" })).toBeEnabled();
});
