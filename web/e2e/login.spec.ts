import { expect, test } from "@playwright/test";
import { E2E_PASSWORD, E2E_USER, mockApi } from "./mock-api";

test.beforeEach(async ({ page }) => {
  await mockApi(page);
});

test("renders the sign-in screen", async ({ page }) => {
  await page.goto("/");

  await expect(page.getByRole("banner").getByText("NSClient++")).toBeVisible();
  await expect(page.getByRole("heading", { name: "Sign in" })).toBeVisible();
  await expect(page.getByLabel("Username")).toBeVisible();
  await expect(page.getByLabel("Password")).toBeVisible();
  await expect(page.getByRole("button", { name: "Sign in" })).toBeVisible();
  // No stray error alert on a fresh load.
  await expect(page.getByRole("alert")).toHaveCount(0);
});

test("shows an error when the credentials are rejected", async ({ page }) => {
  await page.goto("/");

  await page.getByLabel("Username").fill(E2E_USER);
  await page.getByLabel("Password").fill("wrong-password");
  await page.getByRole("button", { name: "Sign in" }).click();

  await expect(page.getByRole("alert")).toContainText(
    "Login failed. Please check your credentials and try again.",
  );
  // Still on the login screen.
  await expect(page.getByRole("heading", { name: "Sign in" })).toBeVisible();
});

test("signs in and lands on the dashboard", async ({ page }) => {
  await page.goto("/");

  await page.getByLabel("Username").fill(E2E_USER);
  await page.getByLabel("Password").fill(E2E_PASSWORD);
  await page.getByRole("button", { name: "Sign in" }).click();

  // The authenticated shell replaces the login card.
  await expect(page.getByRole("heading", { name: "Sign in" })).toHaveCount(0);
  await expect(page.getByText("Dashboard").first()).toBeVisible();
  await expect(page.getByText("CPU Load")).toBeVisible();
  // The app bar now shows the server version delivered by the API.
  await expect(page.getByText("0.17.0")).toBeVisible();
});

test("submits the login form with the Enter key", async ({ page }) => {
  await page.goto("/");

  await page.getByLabel("Username").fill(E2E_USER);
  await page.getByLabel("Password").fill(E2E_PASSWORD);
  await page.getByLabel("Password").press("Enter");

  await expect(page.getByText("CPU Load")).toBeVisible();
});
